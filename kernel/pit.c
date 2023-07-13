#include "debug.h"
#include "pic.h"
#include <io.h>

#define TIMER_FREQUENCY     100
#define REG_CHAN0_DATA      0x40
#define REG_CHAN1_DATA      0x41
#define REG_CHAN2_DATA      0x42
#define REG_COMMAND         0x43

#define COMMAND_BYTE        0x43

static uint64_t ticks = 0;

static void irq_handler()
{
    ticks++;
    uint32_t truncated = ticks & 0xFFFFFFFF;
    if((truncated % 100) == 0) {
        TRACE("ticks: %u", truncated);
    }
}

void pit_init()
{
    int divisor = 1193180 / TIMER_FREQUENCY;
    outb(REG_COMMAND, COMMAND_BYTE);
    outb(REG_CHAN0_DATA, divisor & 0xFF);
    outb(REG_CHAN0_DATA, divisor >> 8);
    pic_set_irq_handler(0, irq_handler);
}


