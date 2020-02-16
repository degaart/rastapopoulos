#include "pit.h"
#include "debug.h"
#include "pic.h"
#include "io.h"
#include "queue.h"
#include "kmalloc.h"
#include "registers.h"

#define PIT_REG_COMMAND     0x43
#define PIT_OCW_COUNTER_1   0x40

/* Timer frequency in Hz */
#define FREQUENCY 1000

struct timer_node {
    uint64_t period;
    timer_handler_t handler;
    void* param;
    uint64_t deadline;
    TAILQ_ENTRY(timer_node) next;
};

static uint64_t _ticks;
static TAILQ_HEAD(timers, timer_node) timers;

static
void timer_handler(int irq, const struct isr_regs* regs)
{
    assert(!interrupts_enabled());

    _ticks++;
    //trace("Uptime: %dms", pit_get_clock());

    uint64_t now = pit_get_clock();
    struct timer_node* node;
    TAILQ_FOREACH(node, &timers, next) {
        if(node->deadline <= now) {
            node->handler(node->param);
            node->deadline = node->deadline + node->period;
        }
    }
}

void pit_init()
{
    pic_install(IRQ_TIMER, timer_handler);
    irq_unmask(IRQ_TIMER);

    int divisor = 1193180 / FREQUENCY;
    outb(PIT_REG_COMMAND, 0x36);        /* squarewave */
    outb(PIT_OCW_COUNTER_1, divisor & 0xFF);
    outb(PIT_OCW_COUNTER_1, (divisor >> 8) & 0xFF);

    TAILQ_INIT(&timers);
}

uint64_t pit_get_ticks()
{
    return _ticks;
}

/*
 * Get time since boot in milliseconds
 * Empirical data suggest we can't go lower in resolution anyway
 */
uint64_t pit_get_clock()
{
    return (1000 * _ticks) / FREQUENCY;
}

void* pit_add_timer(uint64_t period, timer_handler_t handler, void* param)
{
    struct timer_node* node = kmalloc(sizeof(struct timer_node));
    node->period = period;
    node->handler = handler;
    node->param = param;
    node->deadline = pit_get_clock() + period;
    TAILQ_INSERT_TAIL(&timers, node, next);
    return node;
}

uint64_t pit_tick_length()
{
    return 1000 / FREQUENCY;
}

