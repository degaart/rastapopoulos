#include "pit.h"
#include "kernel.h"
#include "kthread.h"
#include "pic.h"
#include "sysinfo.h"
#include "trace.h"
#include "vga.h"
#include <stdio.h>
#include <string.h>

/* PIT Ports */
#define PIT_CHANNEL0_DATA 0x40
#define PIT_CHANNEL1_DATA 0x41
#define PIT_CHANNEL2_DATA 0x42
#define PIT_COMMAND       0x43

/* PIT Input Clock */
#define PIT_BASE_FREQUENCY 1193182

/* Channel Selection */
#define PIT_CHANNEL0 0x00
#define PIT_CHANNEL1 0x40
#define PIT_CHANNEL2 0x80

/* Access Mode */
#define PIT_LATCH           0x00
#define PIT_ACCESS_LOBYTE   0x10
#define PIT_ACCESS_HIBYTE   0x20
#define PIT_ACCESS_LOHIBYTE 0x30

/* Operating Modes */
#define PIT_MODE0 0x00
#define PIT_MODE1 0x02
#define PIT_MODE2 0x04
#define PIT_MODE3 0x06
#define PIT_MODE4 0x08
#define PIT_MODE5 0x0A

/* Counting Mode */
#define PIT_BINARY 0x00
#define PIT_BCD    0x01

volatile uint64_t ticks = 0;

/*
 * WARNING: Do not run time-hungry operations in this IRQ handler
 */
static void pit_irq_handler(int irq, struct isr_regs* regs)
{
    ticks++;
    if (ticks == 1)
        trace("First IRQ0 received");
    event_signal(sysinfo_event);
    pic_eoi(irq);

    char thread_name[16];
    size_t len = current_thread_name(thread_name, sizeof(thread_name));
    for (size_t i = VGA_WIDTH - sizeof(thread_name); i < VGA_WIDTH - len; i++)
        vga_write_at(i, 0, ' ', VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    for (size_t i = 0; i < len; i++) {
        vga_write_at(VGA_WIDTH - len + i, 0, thread_name[i], VGA_COLOR_WHITE,
                     VGA_COLOR_BLUE);
    }

    schedule();
}

void pit_init(uint32_t frequency)
{
    uint16_t divisor = PIT_BASE_FREQUENCY / frequency;

    /* tell pit how we send the divisor value and the mode*/
    outb(PIT_COMMAND,
         PIT_ACCESS_LOHIBYTE | PIT_MODE3 | PIT_CHANNEL0 | PIT_BINARY);
    io_wait();

    /* write low and high bytes respectively */
    outb(PIT_CHANNEL0_DATA, divisor & 0xFF);
    io_wait();
    outb(PIT_CHANNEL0_DATA, divisor >> 8);
    io_wait();

    pic_set_irq_handler(IRQ_TIMER, pit_irq_handler);
}

uint64_t pit_get_ticks(void)
{
    bool const flag = enter_critical_section();
    uint64_t const result = ticks;
    leave_critical_section(flag);
    return result;
}

