#include "ps2kbd.h"
#include "idt.h"
#include "kernel.h"
#include "kthread.h"
#include "pic.h"

#define PS2_DATA    0x60
#define PS2_COMMAND 0x64

#define ENABLE_SCANNING  0xF4
#define DISABLE_SCANNING 0xF5
#define SET_LED          0xED

#define KEY_RELEASED      0x80
#define EXTENDED_SCANCODE 0xE0

static void ps2kbd_isr_handler(int num, struct isr_regs* regs)
{
    uint8_t scancode = inb(PS2_DATA);
    if (scancode == EXTENDED_SCANCODE)
        scancode = inb(PS2_DATA);

    if (scancode & KEY_RELEASED)
        thread_dump();
    pic_eoi(num);
}

void ps2kbd_init(void)
{
    outb(PS2_DATA, ENABLE_SCANNING);
    pic_set_irq_handler(IRQ_KEYBOARD, ps2kbd_isr_handler);
}

