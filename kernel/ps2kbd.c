#include "ps2kbd.h"
#include "event_queue.h"
#include "idt.h"
#include "kernel.h"
#include "kmalloc.h"
#include "pic.h"
#include "process.h"
#include "ringbuf.h"
#include "trace.h"
#include <string.h>

#define PS2_DATA    0x60
#define PS2_COMMAND 0x64
#define OUTPUT_FULL 0x01
#define INPUT_FULL  0x02
#define AUX_DATA    0x20
#define DATA_ERROR  0xC0
#define ACK         0xFA
#define RESEND      0xFE
#define POLL_LIMIT  1000000

static bool held[512];
static bool extended;
static unsigned pause_index;
static uint8_t const pause_sequence[] = {0xE1, 0x1D, 0x45, 0xE1, 0x9D, 0xC5};

static bool wait_input(void)
{
    for (unsigned i = 0; i < POLL_LIMIT; ++i) {
        if (!(inb(PS2_COMMAND) & INPUT_FULL))
            return true;
        io_wait();
    }
    return false;
}

static bool controller_command(uint8_t command)
{
    if (!wait_input())
        return false;
    outb(PS2_COMMAND, command);
    return true;
}

static bool write_data(uint8_t data)
{
    if (!wait_input())
        return false;
    outb(PS2_DATA, data);
    return true;
}

static bool read_data(uint8_t* data)
{
    for (unsigned i = 0; i < POLL_LIMIT; ++i) {
        uint8_t status = inb(PS2_COMMAND);
        if (status & OUTPUT_FULL) {
            uint8_t byte = inb(PS2_DATA);
            if (status & DATA_ERROR)
                return false;
            if (!(status & AUX_DATA)) {
                *data = byte;
                return true;
            }
        }
        io_wait();
    }
    return false;
}

static bool keyboard_command(uint8_t command)
{
    for (unsigned retry = 0; retry < 3; ++retry) {
        uint8_t response;
        if (!write_data(command) || !read_data(&response))
            return false;
        if (response == ACK)
            return true;
        if (response != RESEND)
            return false;
    }
    return false;
}

static void enqueue(uint16_t scancode, enum KeyEventType type)
{
    struct KeyEvent event = {.scancode = scancode, .type = type};
    process_push_input_event(&event);

    // if (type == KEY_RELEASED)
    //     trace("KEY_RELEASED: 0x%x", scancode);
}

static void decode_scancode(uint8_t byte)
{
    if (pause_index) {
        if (byte == pause_sequence[pause_index]) {
            if (++pause_index == sizeof(pause_sequence)) {
                pause_index = 0;
                enqueue(0xE145, KEY_PRESSED);
                enqueue(0xE145, KEY_RELEASED);
            }
            return;
        }
        pause_index = 0;
        /* A malformed sequence must not swallow the next ordinary key */
    }
    if (byte == 0xE1) {
        extended = false;
        pause_index = 1;
        return;
    }
    if (byte == 0xE0) {
        extended = true;
        return;
    }
    if (byte == 0 || byte == 0xFF || byte == ACK || byte == RESEND) {
        extended = false;
        return;
    }

    uint8_t code = byte & 0x7F;
    bool is_extended = extended;
    bool pressed = !(byte & 0x80);
    extended = false;

    /* Print Screen and some navigation keys carry synthetic Shift bytes */
    if (is_extended && (code == 0x2A || code == 0x36))
        return;
    unsigned index = code + (is_extended ? 256 : 0);
    if (held[index] == pressed)
        return;
    held[index] = pressed;
    uint16_t scancode = code | (is_extended ? 0xE000 : 0);
    enqueue(scancode, pressed ? KEY_PRESSED : KEY_RELEASED);
}

static void ps2kbd_isr_handler(int num, struct isr_regs* regs)
{
    uint8_t status = inb(PS2_COMMAND);

    /* Never read a prefix's continuation before its own interrupt arrives
     * Leave auxiliary bytes for the second-port driver
     */
    if ((status & OUTPUT_FULL) && !(status & AUX_DATA)) {
        uint8_t byte = inb(PS2_DATA);
        if (status & DATA_ERROR) {
            extended = false;
            pause_index = 0;
        } else {
            decode_scancode(byte);
        }
    }
    pic_eoi(num);
}

void ps2kbd_init(void)
{
    enter_critical_section();
    pic_mask_irq(IRQ_KEYBOARD);
    memset(held, 0, sizeof(held));
    extended = false;
    pause_index = 0;

    uint8_t config;

    /* Quiesce the first port before clearing stale BIOS output */
    if (!controller_command(0xAD))
        goto failed;
    unsigned drained;
    for (drained = 0; drained < POLL_LIMIT; ++drained) {
        if (!(inb(PS2_COMMAND) & OUTPUT_FULL))
            break;
        inb(PS2_DATA);
        io_wait();
    }
    if (drained == POLL_LIMIT || !controller_command(0x20) ||
        !read_data(&config))
        goto failed;

    /* Preserve second-port settings. Enable translation to set 1 and the first
     * clock, keeping IRQ1 disabled until all command replies have been
     * consumed
     */
    config = (config & ~0x11) | 0x40;
    if (!controller_command(0x60) || !write_data(config) ||
        !controller_command(0xAE) || !keyboard_command(0xF5))
        goto failed;

    /* F5 restores defaults with scanning disabled, avoiding a full reset's
     * BAT replies and controller side effects on emulators. Translation
     * provides set 1 without requiring set-selection or query commands.
     */
    if (!keyboard_command(0xF4))
        goto failed;

    /* Keyboards repeat in hardware; suppress repeated make codes in
     * decode_scancode so consumers see only transitions
     */
    pic_set_irq_handler(IRQ_KEYBOARD, ps2kbd_isr_handler);
    if (!controller_command(0x60) || !write_data(config | 0x01))
        goto failed;
    leave_critical_section();
    trace("PS/2 keyboard initialized (scan code set 1)");
    return;

failed:
    pic_set_irq_handler(IRQ_KEYBOARD, NULL);
    panic("PS/2 keyboard initialization failed: controller or keyboard setup");
}

