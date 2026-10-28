#include "../kthread.h"
#include "../pit.h"
#include "../process.h"
#include "test.h"

extern volatile uint64_t ticks;

static struct Event signal_event;
static struct Event signal_done;
static uint32_t signal_delay;
static struct Event cancel_started;
static struct Event cancel_event;
static struct Event cancel_gate;
static bool cancel_returned;

static void signal_later(void* unused)
{
    thread_sleep_ms(signal_delay);
    event_signal(&signal_event);
    event_signal(&signal_done);
}

static void canceled_waiter(void* argument)
{
    event_signal(&cancel_started);
    cancel_returned = !event_wait(&cancel_event, (uintptr_t)argument);
}

static void cancel_process(void* unused)
{
    event_wait(&cancel_gate, WAIT_INFINITE);
    process_exit(0);
}

void test_events(void)
{
    trace("*** Event tests started");
    struct Event event = {0};
    CHECK(!event_wait(&event, 0));
    event_signal(&event);
    CHECK(event_wait(&event, 0));
    CHECK(!event_wait(&event, 0));
    event_signal(&event);
    CHECK(event_wait(&event, WAIT_INFINITE));
    uint64_t before = pit_get_ticks();
    CHECK(!event_wait(&event, 21));
    CHECK(pit_get_ticks() - before >= pit_ms_to_ticks(21));
    CHECK(event.waiter == NULL);
    event_signal(&event);
    CHECK(event_wait(&event, 0));
    bool flag = enter_critical_section();
    uint64_t saved_ticks = pit_get_ticks();
    uint64_t near_wrap = UINT64_MAX - 1;
    ticks = near_wrap;
    CHECK(!event_wait(&event, 21));
    uint64_t elapsed = pit_get_ticks() - near_wrap;
    CHECK(elapsed >= pit_ms_to_ticks(21));
    ticks = saved_ticks + elapsed;
    leave_critical_section(flag);
    CHECK(pit_ms_to_ticks(1) > 0);
    CHECK(pit_ms_to_ticks(UINT32_MAX) > pit_ms_to_ticks(1000));
    CHECK(pit_ticks_to_ms(UINT64_MAX) == UINT64_MAX);
    for (unsigned i = 0; i < 12; i++) {
        signal_delay = i % 3 == 0 ? 1 : i % 3 == 1 ? 20 : 40;
        CHECK(thread_create(NULL, "event-signal", signal_later, NULL));
        bool flag = enter_critical_section();
        bool result = event_wait(&signal_event, i == 0 ? WAIT_INFINITE : 20);
        CHECK((read_eflags() & EFLAGS_IF) == 0);
        leave_critical_section(flag);
        CHECK(event_wait(&signal_done, 500));
        if (!result)
            CHECK(event_wait(&signal_event, 0));
        CHECK(!event_wait(&signal_event, 0));
        CHECK(signal_event.waiter == NULL);
    }
    for (unsigned i = 0; i < 2; i++) {
        cancel_returned = false;
        struct Process* child = process_create("event-cancel");
        CHECK(child);
        process_incref(child);
        CHECK(thread_create(child, "waiter", canceled_waiter,
                            (void*)(uintptr_t)(i ? WAIT_INFINITE : 10000)));
        CHECK(thread_create(child, "killer", cancel_process, NULL));
        CHECK(event_wait(&cancel_started, 1000));
        event_signal(&cancel_gate);
        CHECK(event_wait(&child->termination_event, 1000));
        CHECK(cancel_returned);
        CHECK(cancel_event.waiter == NULL);
        process_decref(child);
        event_signal(&cancel_event);
        CHECK(event_wait(&cancel_event, 0));
    }
    trace("*** Event tests done");
}
