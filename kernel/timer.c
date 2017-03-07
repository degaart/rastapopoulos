#include "timer.h"
#include "pic.h"
#include "io.h"
#include "util.h"
#include "debug.h"
#include "kernel.h"
#include "locks.h"

#define PORT_COMMAND    0x43
#define PORT_DATA       0x40
#define ICW             0x36
#define INTERNAL_FREQ   1193180
#define FREQ            100              /* HZ */
#define TICKS_PER_MS    (1000 / FREQ)
#define MAX_TIMERS      10

struct timer_info {
    uint32_t id;
    timer_callback_t callback;
    void* callback_data;
    uint32_t period;                    /* milliseconds */
    bool recurring;
    uint64_t last_triggered;
};

static uint64_t current_timestamp = 0;
static uint64_t ticks = 0;
static struct timer_info timers[MAX_TIMERS] = {0};
static int timer_count = 0;
static int next_timer_id = 0;

static void irq_handler(int irq, const struct isr_regs* regs)
{
    ticks++;
    current_timestamp += TICKS_PER_MS;

    // trace("Ticks: %d, Timestamp: %d", (uint32_t)ticks, (uint32_t)current_timestamp);

    struct timer_info triggered[MAX_TIMERS];
    int triggered_idx = 0;
    for(int i = 0; i < timer_count; i++) {
        if(current_timestamp >= timers[i].last_triggered + timers[i].period) {
            timers[i].last_triggered = current_timestamp;
            triggered[triggered_idx++] = timers[i];
        }
    }

    for(int i = 0; i < triggered_idx; i++) {
        triggered[i].callback(triggered[i].callback_data, regs);
        if(!triggered[i].recurring)
            timer_unschedule(triggered[i].id);
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

uint32_t timer_schedule(timer_callback_t callback, void* data, uint32_t period, bool recurring)
{
    assert(timer_count < countof(timers) - 1);

    enter_critical_section();

    uint32_t id = next_timer_id++;
    timers[timer_count].id = id;
    timers[timer_count].callback = callback;
    timers[timer_count].callback_data = data;
    timers[timer_count].period = period;
    timers[timer_count].recurring = recurring;

    timer_count++;

    leave_critical_section();
    return id;
}

void timer_unschedule(uint32_t id)
{
    enter_critical_section();

    int index = -1;
    for(int i = 0; i < timer_count; i++) {
        if(timers[i].id == id) {
            index = i;
            break;
        }
    }

    if(index != -1) {
        if(index != timer_count) {
            timers[index] = timers[timer_count];
        }
        timer_count--;
    }

    leave_critical_section();
}




