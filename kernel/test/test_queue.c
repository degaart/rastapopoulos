#include "../kthread.h"
#include "../pit.h"
#include "test.h"

static struct WaitQueue queue;

struct QueueWaiter
{
    struct Thread* thread;
    struct Event done;
    unsigned timeout;
    bool returned;
};

static struct QueueWaiter waiters[3];

static void queue_waiter(void* argument)
{
    struct QueueWaiter* waiter = argument;
    queue_wait(&queue, waiter->timeout);
    CHECK(thread_current()->waiting_queue == NULL);
    enter_critical_section();
    waiter->returned = true;
    leave_critical_section();
    event_signal(&waiter->done);
}

static void start_waiter(unsigned index, unsigned timeout)
{
    struct QueueWaiter* waiter = &waiters[index];
    waiter->timeout = timeout;
    waiter->returned = false;
    event_init(&waiter->done);
    waiter->thread = thread_create(NULL, "queue-waiter", queue_waiter, waiter);
    CHECK(waiter->thread);
    uint64_t start = pit_get_ticks();
    for (;;) {
        enter_critical_section();
        bool waiting = waiter->thread->waiting_queue == &queue;
        leave_critical_section();
        if (waiting)
            break;
        CHECK(pit_get_ticks() - start < pit_ms_to_ticks(1000));
        thread_sleep_ms(1);
    }
}

static void finish_waiter(unsigned index)
{
    CHECK(event_wait(&waiters[index].done, 2000));
    CHECK(waiters[index].returned);
    event_destroy(&waiters[index].done);
}

void test_queue(void)
{
    trace("*** Queue tests started");
    STAILQ_INIT(&queue);
    queue_notify(&queue);
    queue_notify_all(&queue);
    CHECK(STAILQ_EMPTY(&queue));

    unsigned timeouts[] = {0, 21};
    for (unsigned i = 0; i < 2; i++) {
        enter_critical_section();
        int nesting = thread_current()->critical_section_nesting;
        uint64_t start = pit_get_ticks();
        queue_wait(&queue, timeouts[i]);
        CHECK(pit_get_ticks() - start >= pit_ms_to_ticks(timeouts[i]));
        CHECK(!(read_eflags() & EFLAGS_IF));
        CHECK(thread_current()->critical_section_nesting == nesting);
        CHECK(thread_current()->waiting_queue == NULL);
        CHECK(STAILQ_EMPTY(&queue));
        leave_critical_section();
    }

    start_waiter(0, WAIT_INFINITE);
    start_waiter(1, WAIT_INFINITE);
    start_waiter(2, WAIT_INFINITE);
    for (unsigned i = 0; i < 3; i++) {
        enter_critical_section();
        CHECK(STAILQ_FIRST(&queue) == waiters[i].thread);
        queue_notify(&queue);
        CHECK(!(read_eflags() & EFLAGS_IF));
        CHECK(waiters[i].thread->waiting_queue == NULL);
        CHECK(waiters[i].thread->state == THREAD_READY);
        for (unsigned j = i + 1; j < 3; j++) {
            CHECK(!waiters[j].returned);
            CHECK(waiters[j].thread->state == THREAD_BLOCKED);
            CHECK(waiters[j].thread->waiting_queue == &queue);
        }
        leave_critical_section();
        finish_waiter(i);
    }
    CHECK(STAILQ_EMPTY(&queue));

    start_waiter(0, WAIT_INFINITE);
    start_waiter(1, 200);
    start_waiter(2, WAIT_INFINITE);
    finish_waiter(1);
    enter_critical_section();
    CHECK(STAILQ_FIRST(&queue) == waiters[0].thread);
    CHECK(STAILQ_NEXT(waiters[0].thread, wait_queue_entry) ==
          waiters[2].thread);
    CHECK(STAILQ_NEXT(waiters[2].thread, wait_queue_entry) == NULL);
    CHECK(!waiters[0].returned);
    CHECK(!waiters[2].returned);
    queue_notify_all(&queue);
    CHECK(!(read_eflags() & EFLAGS_IF));
    CHECK(STAILQ_EMPTY(&queue));
    CHECK(waiters[0].thread->waiting_queue == NULL);
    CHECK(waiters[2].thread->waiting_queue == NULL);
    CHECK(waiters[0].thread->state == THREAD_READY);
    CHECK(waiters[2].thread->state == THREAD_READY);
    leave_critical_section();
    finish_waiter(0);
    finish_waiter(2);
    queue_notify(&queue);
    queue_notify_all(&queue);
    CHECK(STAILQ_EMPTY(&queue));
    trace("*** Queue tests done");
}
