#include "pit.h"
#include "debug.h"
#include "pic.h"
#include "io.h"

#define PIT_REG_COMMAND     0x43
#define PIT_OCW_COUNTER_1   0x40

static uint64_t ticks;

static
void timer_handler(int irq, const struct isr_regs* regs)
{
    trace("Timer");
    ticks++;
}

void pit_init()
{
    pic_install(IRQ_TIMER, timer_handler);
    irq_unmask(IRQ_TIMER);

    int frequency = 10; /* Hz */
    int divisor = 1193180 / frequency;
    outb(PIT_REG_COMMAND, 0x36);        /* squarewave */
    outb(PIT_OCW_COUNTER_1, divisor & 0xFF);
    outb(PIT_OCW_COUNTER_1, (divisor >> 8) & 0xFF);
}



