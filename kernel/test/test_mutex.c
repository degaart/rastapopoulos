#include "../fdc.h"
#include "../mutex.h"
#include "../pit.h"
#include "../process.h"
#include "test.h"

static struct Mutex mutex;
static unsigned completed;
static unsigned active;
static struct Event cancel_gate;
static bool cancel_returned;
static char io_data[512];

/* Run separately: the non-owner unlock must panic instead of returning. */
#ifndef MUTEX_NEGATIVE_CASE
#    define MUTEX_NEGATIVE_CASE 0
#endif

struct MutexWaiter
{
    struct Thread* thread;
    struct Event done;
};

static struct MutexWaiter waiters[3];

static void contending_waiter(void* argument)
{
    struct MutexWaiter* waiter = argument;
    struct Thread* owner = mutex.owner;
    unsigned owned = thread_current()->owned_mutexes;
    CHECK(!mutex_trylock(&mutex));
    CHECK(mutex.owner == owner);
    CHECK(thread_current()->owned_mutexes == owned);
    CHECK(thread_current()->waiting_queue == NULL);
    enter_critical_section();
    int nesting = thread_current()->critical_section_nesting;
    CHECK(mutex_lock(&mutex));
    CHECK(thread_current()->critical_section_nesting == nesting);
    CHECK(!(read_eflags() & EFLAGS_IF));
    CHECK(mutex.owner == thread_current());
    CHECK(thread_current()->waiting_queue == NULL);
    leave_critical_section();
    CHECK(read_eflags() & EFLAGS_IF);
    CHECK(thread_current()->critical_section_nesting == 0);
    CHECK(thread_current()->owned_mutexes == owned + 1);
    CHECK(completed == (unsigned)(waiter - waiters));
    CHECK(active == 0);
    active++;
    thread_sleep_ms(10);
    CHECK(read_eflags() & EFLAGS_IF);
    CHECK(active == 1);
    completed++;
    active--;
    mutex_unlock(&mutex);
    CHECK(read_eflags() & EFLAGS_IF);
    CHECK(thread_current()->owned_mutexes == owned);
    event_signal(&waiter->done);
}

static void wait_until_blocked(struct Thread* thread)
{
    uint64_t start = pit_get_ticks();
    for (;;) {
        enter_critical_section();
        bool waiting = thread->waiting_queue == &mutex.wait_queue;
        if (waiting) {
            CHECK(thread->state == THREAD_BLOCKED);
            CHECK(thread->sleep_deadline == 0);
            CHECK(thread->owned_mutexes == 0);
        }
        leave_critical_section();
        if (waiting)
            return;
        CHECK(pit_get_ticks() - start < pit_ms_to_ticks(1000));
        thread_sleep_ms(1);
    }
}

static void canceled_waiter(void* unused)
{
    unsigned owned = thread_current()->owned_mutexes;
    cancel_returned = !mutex_lock(&mutex);
    CHECK(thread_current()->owned_mutexes == owned);
    CHECK(mutex.owner != thread_current());
    CHECK(thread_current()->waiting_queue == NULL);
}

static void cancel_process(void* unused)
{
    CHECK(event_wait(&cancel_gate, WAIT_INFINITE));
    process_exit(0);
}

#if MUTEX_NEGATIVE_CASE
static void nonowner_unlock(void* unused)
{
    CHECK(mutex.owner != thread_current());
    trace("Expect panic: non-owner mutex unlock");
    mutex_unlock(&mutex);
    CHECK(false);
}
#endif

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

#if MUTEX_NEGATIVE_CASE
    CHECK(mutex_lock(&mutex));
    CHECK(thread_create(NULL, "mutex-nonowner", nonowner_unlock, NULL));
    thread_sleep_ms(1000);
    CHECK(false);
#endif

    CHECK(read_eflags() & EFLAGS_IF);
    CHECK(mutex_trylock(&mutex));
    CHECK(read_eflags() & EFLAGS_IF);
    mutex_unlock(&mutex);
    CHECK(read_eflags() & EFLAGS_IF);

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
    for (unsigned sample = 0; sample < 5; sample++) {
        thread_sleep_ms(10);
        enter_critical_section();
        for (unsigned i = 0; i < 3; i++) {
            CHECK(waiters[i].thread->state == THREAD_BLOCKED);
            CHECK(waiters[i].thread->waiting_queue == &mutex.wait_queue);
            CHECK(waiters[i].thread->sleep_deadline == 0);
        }
        leave_critical_section();
    }
    CHECK(read_eflags() & EFLAGS_IF);
    CHECK(fdc_read(0, 0, io_data, sizeof(io_data)));
    CHECK((unsigned char)io_data[510] == 0x55);
    CHECK((unsigned char)io_data[511] == 0xaa);
    CHECK(read_eflags() & EFLAGS_IF);
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

    completed = 0;
    event_init(&cancel_gate);
    cancel_returned = false;
    CHECK(mutex_lock(&mutex));
    event_init(&waiters[0].done);
    waiters[0].thread =
        thread_create(NULL, "mutex-first", contending_waiter, &waiters[0]);
    CHECK(waiters[0].thread);
    wait_until_blocked(waiters[0].thread);
    struct Process* child = process_create("mutex-cancel");
    CHECK(child);
    process_incref(child);
    struct Thread* waiter =
        thread_create(child, "mutex-waiter", canceled_waiter, NULL);
    CHECK(waiter);
    CHECK(thread_create(child, "mutex-killer", cancel_process, NULL));
    wait_until_blocked(waiter);
    event_init(&waiters[1].done);
    waiters[1].thread =
        thread_create(NULL, "mutex-last", contending_waiter, &waiters[1]);
    CHECK(waiters[1].thread);
    wait_until_blocked(waiters[1].thread);
    event_signal(&cancel_gate);
    CHECK(event_wait(&child->termination_event, 2000));
    CHECK(cancel_returned);
    CHECK(mutex.owner == thread_current());
    enter_critical_section();
    CHECK(STAILQ_FIRST(&mutex.wait_queue) == waiters[0].thread);
    CHECK(STAILQ_NEXT(waiters[0].thread, wait_queue_entry) ==
          waiters[1].thread);
    CHECK(STAILQ_NEXT(waiters[1].thread, wait_queue_entry) == NULL);
    leave_critical_section();
    process_decref(child);
    event_destroy(&cancel_gate);
    mutex_unlock(&mutex);
    for (unsigned i = 0; i < 2; i++) {
        CHECK(event_wait(&waiters[i].done, 2000));
        event_destroy(&waiters[i].done);
    }
    CHECK(completed == 2);
    CHECK(mutex.owner == NULL);
    CHECK(STAILQ_EMPTY(&mutex.wait_queue));
    CHECK(mutex_trylock(&mutex));
    mutex_unlock(&mutex);
    trace("*** Mutex tests done");
}
