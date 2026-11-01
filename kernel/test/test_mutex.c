#include "../mutex.h"
#include "../pit.h"
#include "../process.h"
#include "test.h"

static struct Mutex mutex;
static unsigned completed;
static unsigned active;
static struct Event cancel_gate;
static bool cancel_returned;

struct MutexWaiter
{
    struct Thread* thread;
    struct Event done;
};

static struct MutexWaiter waiters[3];

static void contending_waiter(void* argument)
{
    struct MutexWaiter* waiter = argument;
    CHECK(!mutex_trylock(&mutex));
    enter_critical_section();
    int nesting = thread_current()->critical_section_nesting;
    CHECK(mutex_lock(&mutex));
    CHECK(thread_current()->critical_section_nesting == nesting);
    CHECK(!(read_eflags() & EFLAGS_IF));
    CHECK(mutex.owner == thread_current());
    CHECK(thread_current()->waiting_queue == NULL);
    leave_critical_section();
    CHECK(active == 0);
    active++;
    thread_sleep_ms(10);
    CHECK(active == 1);
    completed++;
    active--;
    mutex_unlock(&mutex);
    event_signal(&waiter->done);
}

static void wait_until_blocked(struct Thread* thread)
{
    uint64_t start = pit_get_ticks();
    for (;;) {
        enter_critical_section();
        bool waiting = thread->waiting_queue == &mutex.wait_queue;
        leave_critical_section();
        if (waiting)
            return;
        CHECK(pit_get_ticks() - start < pit_ms_to_ticks(1000));
        thread_sleep_ms(1);
    }
}

static void canceled_waiter(void* unused)
{
    cancel_returned = !mutex_lock(&mutex);
    CHECK(mutex.owner != thread_current());
    CHECK(thread_current()->waiting_queue == NULL);
}

static void cancel_process(void* unused)
{
    CHECK(event_wait(&cancel_gate, WAIT_INFINITE));
    process_exit(0);
}

void test_mutex(void)
{
    trace("*** Mutex tests started");
    mutex_init(&mutex);
    CHECK(mutex.owner == NULL);
    CHECK(STAILQ_EMPTY(&mutex.wait_queue));

    enter_critical_section();
    int nesting = thread_current()->critical_section_nesting;
    CHECK(mutex_lock(&mutex));
    CHECK(mutex.owner == thread_current());
    CHECK(!mutex_lock(&mutex));
    CHECK(!mutex_trylock(&mutex));
    CHECK(mutex.owner == thread_current());
    CHECK(STAILQ_EMPTY(&mutex.wait_queue));
    mutex_unlock(&mutex);
    CHECK(mutex.owner == NULL);
    CHECK(mutex_trylock(&mutex));
    CHECK(mutex.owner == thread_current());
    mutex_unlock(&mutex);
    CHECK(thread_current()->critical_section_nesting == nesting);
    CHECK(!(read_eflags() & EFLAGS_IF));
    leave_critical_section();

    completed = 0;
    active = 0;
    CHECK(mutex_lock(&mutex));
    for (unsigned i = 0; i < 3; i++) {
        event_init(&waiters[i].done);
        waiters[i].thread = thread_create(NULL, "mutex-waiter",
                                          contending_waiter, &waiters[i]);
        CHECK(waiters[i].thread);
        wait_until_blocked(waiters[i].thread);
    }
    CHECK(completed == 0);
    CHECK(mutex.owner == thread_current());
    mutex_unlock(&mutex);
    for (unsigned i = 0; i < 3; i++) {
        CHECK(event_wait(&waiters[i].done, 2000));
        event_destroy(&waiters[i].done);
    }
    CHECK(completed == 3);
    CHECK(active == 0);
    CHECK(mutex.owner == NULL);
    CHECK(STAILQ_EMPTY(&mutex.wait_queue));

    event_init(&cancel_gate);
    cancel_returned = false;
    CHECK(mutex_lock(&mutex));
    struct Process* child = process_create("mutex-cancel");
    CHECK(child);
    process_incref(child);
    struct Thread* waiter =
        thread_create(child, "mutex-waiter", canceled_waiter, NULL);
    CHECK(waiter);
    CHECK(thread_create(child, "mutex-killer", cancel_process, NULL));
    wait_until_blocked(waiter);
    event_signal(&cancel_gate);
    CHECK(event_wait(&child->termination_event, 2000));
    CHECK(cancel_returned);
    CHECK(mutex.owner == thread_current());
    CHECK(STAILQ_EMPTY(&mutex.wait_queue));
    process_decref(child);
    event_destroy(&cancel_gate);
    mutex_unlock(&mutex);
    CHECK(mutex_trylock(&mutex));
    mutex_unlock(&mutex);
    trace("*** Mutex tests done");
}
