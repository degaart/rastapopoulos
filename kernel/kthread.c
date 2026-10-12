#include "kthread.h"
#include "kernel.h"
#include "kmalloc.h"
#include "trace.h"
#include <assert.h>
#include <string.h>

#define SCHEDULER_QUANTUM 1

static STAILQ_HEAD(ThreadList,
                   Thread) ready_queue = STAILQ_HEAD_INITIALIZER(ready_queue);
static STAILQ_HEAD(ThreadList, Thread)
    terminated_queue = STAILQ_HEAD_INITIALIZER(terminated_queue);
static struct Thread* current_thread;
static struct Thread* idle_thread;
static uint64_t scheduler_ticks;

void scheduler_enqueue(struct Thread* thread)
{
    assert(thread != idle_thread);

    bool flag = enter_critical_section();
    thread->state = THREAD_READY;
    STAILQ_INSERT_TAIL(&ready_queue, thread, entry);
    leave_critical_section(flag);
}

struct Thread* scheduler_dequeue(void)
{
    bool flag = enter_critical_section();
    struct Thread* next = STAILQ_FIRST(&ready_queue);
    if (next)
        STAILQ_REMOVE_HEAD(&ready_queue, entry);
    else
        next = idle_thread;
    leave_critical_section(flag);
    return next;
}

void schedule(void)
{
    scheduler_ticks++;
    if (scheduler_ticks >= SCHEDULER_QUANTUM) {
        scheduler_ticks = 0;
        if (!STAILQ_EMPTY(&ready_queue))
            thread_yield();
    }
}

/* Create initial thread and makes it the idle thread */
void thread_init(void)
{
    struct Thread* thread = kmalloc(sizeof(struct Thread));
    if (!thread)
        panic("Out of memory");
    memset(thread, 0, sizeof(*thread));
    thread->stack =
        NULL; /* It really is initial_stack_top but we should never free it */
    thread->state = THREAD_RUNNING;
    idle_thread = thread;
    current_thread = thread;
}

static void cleanup_terminated(void);

static void thread_start(void (*entry)(void*), void* arg)
{
    cleanup_terminated();
    entry(arg);
    thread_exit();
}

struct Thread* thread_create(void (*entry)(void*), void* arg)
{
    bool const flag = enter_critical_section();
    struct Thread* thread = kmalloc(sizeof(struct Thread));
    if (!thread) {
        leave_critical_section(flag);
        return NULL;
    }
    memset(thread, 0, sizeof(*thread));
    thread->stack = kmalloc(4096);
    if (!thread->stack) {
        kfree(thread);
        leave_critical_section(flag);
        return NULL;
    }

    uint32_t* esp = thread->stack + 4096 - sizeof(uint32_t);
    *esp-- = (uintptr_t)arg;
    *esp-- = (uintptr_t)entry;
    *esp-- = (uintptr_t)thread_exit;
    *esp-- = (uintptr_t)thread_start; /* return address */
    *esp-- = 0;                       /* ebp */
    *esp-- = 0;                       /* ebx */
    *esp-- = 0;                       /* esi */
    *esp-- = 0;                       /* edi */
    *esp = read_eflags() | EFLAGS_IF;
    thread->esp = (uint32_t)(uintptr_t)esp;

    scheduler_enqueue(thread);
    leave_critical_section(flag);
    return thread;
}

static void cleanup_terminated(void)
{
    bool flag = enter_critical_section();
    while (!STAILQ_EMPTY(&terminated_queue)) {
        struct Thread* thread = STAILQ_FIRST(&terminated_queue);
        STAILQ_REMOVE_HEAD(&terminated_queue, entry);
        assert(thread != idle_thread);
        kfree(thread->stack);
        kfree(thread);
    }
    leave_critical_section(flag);
}

void thread_yield(void)
{
    bool const flag = enter_critical_section();
    struct Thread* next = scheduler_dequeue();

    if (next != current_thread) {
        next->state = THREAD_RUNNING;

        struct Thread* old = current_thread;
        current_thread = next;

        if (old != idle_thread)
            scheduler_enqueue(old);
        context_switch(&old->esp, next->esp);
    }

    cleanup_terminated();
    leave_critical_section(flag);
}

void thread_exit(void)
{
    if (current_thread == idle_thread)
        panic("Idle thread exited");

    enter_critical_section();
    struct Thread* next = scheduler_dequeue();
    assert(next != current_thread);

    next->state = THREAD_RUNNING;
    current_thread->state = THREAD_TERMINATED;
    STAILQ_INSERT_TAIL(&terminated_queue, current_thread, entry);
    current_thread = next;
    scheduler_ticks = 0;

    uint32_t old_esp;
    context_switch(&old_esp, next->esp);

    panic("Terminated thread resumed");
}

void thread_dump(void)
{
    struct Thread* first = STAILQ_FIRST(&ready_queue);
    if (first) {
        trace("Ready queue: 0x%p", first);
    } else {
        trace("Ready queue empty");
    }
    trace("current_thread: 0x%p, idle_thread: 0x%p", current_thread,
          idle_thread);
}

struct Event* event_create(void)
{
    struct Event* event = kmalloc(sizeof(struct Event));
    memset(event, 0, sizeof(*event));
    return event;
}

void event_free(struct Event* event)
{
    kfree(event);
}

void event_wait(struct Event* event)
{
    bool iflag = enter_critical_section();
    while (!event->pending) {
        event->waiter = current_thread;
        current_thread->state = THREAD_BLOCKED;

        struct Thread* next = scheduler_dequeue();
        assert(next != current_thread);

        next->state = THREAD_RUNNING;
        current_thread = next;
        context_switch(&event->waiter->esp, next->esp);
    }
    event->pending = false;
    leave_critical_section(iflag);
}

void event_signal(struct Event* event)
{
    bool iflag = enter_critical_section();
    event->pending = true;
    if (event->waiter != NULL) {
        struct Thread* waiter = event->waiter;
        assert(waiter->state == THREAD_BLOCKED);
        event->waiter = NULL;
        scheduler_enqueue(waiter);
    }
    leave_critical_section(iflag);
}

