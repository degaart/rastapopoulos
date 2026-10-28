#include "pit.h"
#include "kernel.h"
#include "kthread.h"
#include "pic.h"
#include "process.h"
#include "trace.h"
#include "vga.h"
#include <assert.h>
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
static uint32_t tick_divisor;
static uint16_t perf_previous;
static uint64_t perf_clocks;

uint64_t pit_perf_clock(void)
{
    bool flag = enter_critical_section();
    outb(PIT_COMMAND, PIT_CHANNEL2 | PIT_LATCH);
    unsigned low = inb(PIT_CHANNEL2_DATA);
    uint16_t counter = low | ((unsigned)inb(PIT_CHANNEL2_DATA) << 8);
    perf_clocks += (uint16_t)(perf_previous - counter);
    perf_previous = counter;
    uint64_t result = perf_clocks;
    leave_critical_section(flag);
    return result;
}

/*
 * WARNING: Do not run time-hungry operations in this IRQ handler
 */
static void pit_irq_handler(int irq, struct isr_regs* regs)
{
    pit_perf_clock();
    ticks++;
    if (ticks == 1)
        trace("First IRQ0 received");
    pic_eoi(irq);

    // char process_buffer[32];
    // process_get_info(process_buffer, sizeof(process_buffer));

    // char thread_buffer[32];
    // thread_get_info(thread_buffer, sizeof(thread_buffer));

    // char buffer[81];
    // size_t len = snprintf(buffer, sizeof(buffer), "%llu %s %s", ticks,
    //                       process_buffer, thread_buffer);
    // int i;
    // for (i = 0; i < 80 && i < len; i++) {
    //     vga_write_at(i, 0, buffer[i], VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    // }
    // for (; i < 80; i++) {
    //     vga_write_at(i, 0, ' ', VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    // }

    schedule();
}

void pit_init(uint32_t frequency)
{
    assert(frequency > 0 && frequency <= PIT_BASE_FREQUENCY);
    tick_divisor = PIT_BASE_FREQUENCY / frequency;
    assert(tick_divisor <= UINT16_MAX);
    uint16_t divisor = tick_divisor;
    outb(0x61, (inb(0x61) & ~3u) | 1u);
    outb(PIT_COMMAND, PIT_CHANNEL2 | PIT_ACCESS_LOHIBYTE | PIT_MODE2);
    outb(PIT_CHANNEL2_DATA, 0);
    outb(PIT_CHANNEL2_DATA, 0);
    pit_perf_clock();

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

uint64_t pit_ms_to_ticks(uint32_t milliseconds)
{
    uint64_t denominator = (uint64_t)tick_divisor * 1000;
    return ((uint64_t)milliseconds * PIT_BASE_FREQUENCY + denominator - 1) /
           denominator;
}

uint64_t pit_ticks_to_ms(uint64_t count)
{
    uint64_t multiplier = (uint64_t)tick_divisor * 1000;
    uint64_t whole = count / PIT_BASE_FREQUENCY;
    uint64_t tail =
        (count % PIT_BASE_FREQUENCY * multiplier + PIT_BASE_FREQUENCY - 1) /
        PIT_BASE_FREQUENCY;
    if (whole > (UINT64_MAX - tail) / multiplier)
        return UINT64_MAX;
    return whole * multiplier + tail;
}
