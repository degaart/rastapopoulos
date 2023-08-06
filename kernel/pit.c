#include "debug.h"
#include "kmalloc.h"
#include "pic.h"
#include <io.h>

#define TIMER_FREQUENCY 1000
#define REG_CHAN0_DATA  0x40
#define REG_CHAN1_DATA  0x41
#define REG_CHAN2_DATA  0x42
#define REG_COMMAND     0x43

#define COMMAND_BYTE 0x43

typedef void (*timer_t)(uint64_t, void*);

struct timer {
    timer_t handler;
    void* ctx;
    unsigned interval;
    unsigned elapsed;
    struct timer* next;
};

static uint64_t ticks = 0;
static struct timer* timers;

uint64_t get_ticks(void)
{
    return ticks;
}

void pit_add_timer(timer_t handler, void* ctx, unsigned interval)
{
    struct timer* timer = kmalloc(sizeof(struct timer));
    timer->handler = handler;
    timer->ctx = ctx;
    timer->interval = interval;
    timer->elapsed = 0;
    timer->next = timers;
    timers = timer;
}

static void irq_handler()
{
    ticks++;
    for(struct timer* timer = timers; timer; timer = timer->next) {
        if(timer) {
            timer->elapsed += 1000 / TIMER_FREQUENCY;
            if(timer->elapsed >= timer->interval) {
                timer->elapsed -= timer->interval;
                timer->handler(ticks, timer->ctx);
            }
        }
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
