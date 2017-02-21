#include "timer.h"
#include "pic.h"
#include "io.h"
#include "util.h"
#include "debug.h"
#include "kernel.h"

#define PORT_COMMAND    0x43
#define PORT_DATA       0x40
#define ICW             0x36
#define INTERNAL_FREQ   1193180
#define FREQ            25              /* HZ */
#define TICKS_PER_MS    (1000 / FREQ)

static uint64_t current_timestamp = 0;
static uint64_t ticks = 0;

static void irq_handler(int irq, const struct isr_regs* regs)
{
    ticks++;
    current_timestamp += TICKS_PER_MS;

    /* TODO: Handle scheduled timers */
    trace("Ticks: %d, Timestamp: %d", (uint32_t)ticks, (uint32_t)current_timestamp);

    if(ticks >= 10) {
        reboot();
    }
}

void timer_init()
{
    uint32_t divisor = INTERNAL_FREQ / FREQ;

    outb(PORT_COMMAND, ICW);
    outb(PORT_DATA, LOBYTE(divisor));
    outb(PORT_DATA, HIBYTE(divisor));

    pic_install(IRQ_TIMER, irq_handler);
}

uint64_t timer_timestamp()
{
    return current_timestamp;
}

uint64_t timer_ticks()
{
    return ticks;
}


