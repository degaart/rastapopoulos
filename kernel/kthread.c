#include "kthread.h"
#include "kernel.h"
#include "kmalloc.h"
#include "pit.h"
#include "pmm.h"
#include "process.h"
#include "trace.h"
#include "tss.h"
#include <assert.h>
#include <gdt.h>
#include <string.h>

#define SCHEDULER_QUANTUM 10
#define DEADLINE_INFINITE 0

static struct ThreadList ready_queue = STAILQ_HEAD_INITIALIZER(ready_queue);
static struct ThreadList terminated_queue =
    STAILQ_HEAD_INITIALIZER(terminated_queue);
static struct ThreadList sleeping_queue =
    STAILQ_HEAD_INITIALIZER(sleeping_queue);
static struct Thread* current_thread;
static struct Thread* idle_thread;
static uint64_t scheduler_ticks;
extern struct Process* kernel_task;

void scheduler_enqueue(struct Thread* thread)
{
    if (thread == idle_thread)
        return;

    enter_critical_section();
    thread->state = THREAD_READY;
    STAILQ_INSERT_TAIL(&ready_queue, thread, threads);
    leave_critical_section();
}

struct Thread* scheduler_dequeue(void)
{
    enter_critical_section();
    struct Thread* next = STAILQ_FIRST(&ready_queue);
    if (next)
        STAILQ_REMOVE_HEAD(&ready_queue, threads);
    else
        next = idle_thread;
    leave_critical_section();
    return next;
}

static void schedule_next()
{
    assert(!(read_eflags() & EFLAGS_IF));
    struct Thread* next = scheduler_dequeue();
    next->state = THREAD_RUNNING;
    if (current_thread == next)
        return;

    struct Thread* old = current_thread;
    current_thread = next;
    scheduler_ticks = 0;

    process_set_current(next->process);
    tss.esp0 = (uintptr_t)next->stack_top;
    context_switch(&old->esp, next->esp);
}

void schedule(void)
{
    assert((read_eflags() & EFLAGS_IF) == 0);

    uint64_t current_ticks = pit_get_ticks();

    /*
     * STAILQ_REMOVE scans from head, which can have a perf impact when used
     * with STAILQ_FOREACH_SAFE
     * Instead, we scan linearly and use STAILQ_REMOVE_AFTER
     * TODO: Replace STAILQ with TAILQ
     */
    struct Thread* prev = NULL;
    struct Thread* thread = STAILQ_FIRST(&sleeping_queue);
    while (thread) {
        struct Thread* next = STAILQ_NEXT(thread, threads);
        if (thread->process->exit_requested ||
            (thread->sleep_deadline != DEADLINE_INFINITE &&
             thread->sleep_deadline <= current_ticks)) {
            if (prev)
                STAILQ_REMOVE_AFTER(&sleeping_queue, prev, threads);
            else
                STAILQ_REMOVE_HEAD(&sleeping_queue, threads);
            scheduler_enqueue(thread);
        } else {
            prev = thread;
        }
        thread = next;
    }

    scheduler_ticks++;
    if (current_thread == idle_thread && !STAILQ_EMPTY(&ready_queue)) {
        thread_yield();
    } else if (scheduler_ticks >= SCHEDULER_QUANTUM) {
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
    strlcpy(thread->name, "idle", sizeof(thread->name));

    thread->stack_size = 16384;
    thread->stack = initial_stack;
    thread->stack_top = initial_stack_top;
    thread->state = THREAD_RUNNING;
    idle_thread = thread;
    current_thread = thread;

    struct Process* proc = process_current();
    assert(proc != NULL);
    process_add_thread(proc, thread);
}

static void cleanup_terminated(void);

static void thread_start(void (*entry)(void*), void* arg)
{
    cleanup_terminated();
    entry(arg);
    thread_exit();
}

struct Thread* thread_create(struct Process* proc, const char* name,
                             thread_entry_t entry, void* arg)
{
    if (!proc)
        proc = kernel_task;
    if (!proc || !name || !entry)
        return false;

    enter_critical_section();
    if (proc->state != PROCESS_ALIVE) {
        leave_critical_section();
        return NULL;
    }

    struct Thread* thread = kmalloc(sizeof(struct Thread));
    if (!thread) {
        leave_critical_section();
        return NULL;
    }

    memset(thread, 0, sizeof(*thread));
    strlcpy(thread->name, name, sizeof(thread->name));

    thread->stack_size = 4096;
    thread->stack = kmalloc(thread->stack_size);
    if (!thread->stack) {
        kfree(thread);
        leave_critical_section();
        return NULL;
    }
    thread->stack_top = thread->stack + thread->stack_size;

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

    process_add_thread(proc, thread);

    scheduler_enqueue(thread);
    leave_critical_section();
    return thread;
}

static void cleanup_terminated(void)
{
    enter_critical_section();
    while (!STAILQ_EMPTY(&terminated_queue)) {
        struct Thread* thread = STAILQ_FIRST(&terminated_queue);
        trace("Destroying thread \"%s\"", thread->name);

        STAILQ_REMOVE_HEAD(&terminated_queue, threads);
        assert(thread != idle_thread);

        process_remove_thread(thread->process, thread);
        kfree(thread->stack);
        kfree(thread);
    }
    leave_critical_section();
}

void thread_yield(void)
{
    enter_critical_section();
    scheduler_enqueue(current_thread);
    schedule_next();
    cleanup_terminated();
    leave_critical_section();
}

void thread_exit(void)
{
    enter_critical_section();
    if (current_thread == idle_thread)
        panic("Idle thread exited");
    else if (current_thread->owned_mutexes > 0)
        panic("Thread exited while still owning %u mutexes",
              current_thread->owned_mutexes);

    current_thread->state = THREAD_TERMINATED;
    STAILQ_INSERT_TAIL(&terminated_queue, current_thread, threads);
    schedule_next();
    panic("Terminated thread still running");
}

void thread_sleep(uint32_t ticks)
{
    enter_critical_section();
    assert(current_thread != idle_thread);

    if (ticks == 0) {
        if (!STAILQ_EMPTY(&ready_queue)) {
            /* if another thread is ready, switch to it, otherwise return
             * immediately */
            thread_yield();
        }
        leave_critical_section();
        if (current_thread->process->exit_requested)
            thread_exit();
        return;
    }

    uint64_t now = pit_get_ticks();
    if (now + ticks < ticks)
        panic("Tick wraparound");

    current_thread->sleep_deadline = now + ticks;
    if (current_thread->sleep_deadline == DEADLINE_INFINITE)
        current_thread->sleep_deadline++;
    current_thread->state = THREAD_BLOCKED;
    STAILQ_INSERT_TAIL(&sleeping_queue, current_thread, threads);
    schedule_next();
    if (current_thread->process->exit_requested)
        thread_exit();
    leave_critical_section();
}

void thread_sleep_ms(uint32_t milliseconds)
{
    uint64_t ticks = pit_ms_to_ticks(milliseconds);
    if (ticks > UINT32_MAX)
        ticks = UINT32_MAX;
    thread_sleep((uint32_t)ticks);
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

void event_init(struct Event* evt)
{
    memset(evt, 0, sizeof(*evt));
    STAILQ_INIT(&evt->waitqueue);
}

void event_destroy(struct Event* evt)
{
    if (!STAILQ_EMPTY(&evt->waitqueue))
        panic("Destroying an event with a non-empty waitqueue");
}

bool event_wait(struct Event* event, uint32_t timeout_ms)
{
    enter_critical_section();
    if (current_thread->process->exit_requested) {
        leave_critical_section();
        return false;
    }

    if (event->pending) {
        event->pending--;
        leave_critical_section();
        return true;
    }

    if (timeout_ms == 0) {
        leave_critical_section();
        return false;
    }

    while (true) {
        uint64_t start = pit_get_ticks();
        queue_wait(&event->waitqueue, timeout_ms);
        if (current_thread->process->exit_requested) {
            leave_critical_section();
            return false;
        }
        if (event->pending)
            break;

        if (timeout_ms == WAIT_INFINITE)
            continue;

        uint64_t elapsed = pit_ticks_to_ms(pit_get_ticks() - start);
        if (timeout_ms <= elapsed)
            break;
        timeout_ms = timeout_ms - elapsed;
    }

    bool result = event->pending != 0;
    if (event->pending)
        event->pending--;
    leave_critical_section();
    return result;
}

void event_signal(struct Event* event)
{
    enter_critical_section();
    queue_notify(&event->waitqueue);
    event->pending++;
    leave_critical_section();
}

size_t current_thread_name(char* buf, size_t size)
{
    enter_critical_section();
    size_t result = strlcpy(buf, current_thread->name, size);
    leave_critical_section();
    return result;
}

void thread_get_info(char* buffer, size_t len)
{
    if (!len)
        return;

    enter_critical_section();
    snprintf(buffer, len, "CT: %s",
             current_thread ? current_thread->name : "NULL");
    leave_critical_section();
}

struct Thread* thread_current(void)
{
    return current_thread;
}

static void waitqueue_remove_thread(struct WaitQueue* queue,
                                    struct Thread* thread)
{
    enter_critical_section();

    STAILQ_REMOVE(queue, thread, Thread, wait_queue_entry);
    if (thread->state == THREAD_BLOCKED) {
        STAILQ_REMOVE(&sleeping_queue, thread, Thread, threads);
        scheduler_enqueue(thread);
    }
    thread->waiting_queue = NULL;

    leave_critical_section();
}

void queue_init(struct WaitQueue* queue)
{
    STAILQ_INIT(queue);
}

void queue_wait(struct WaitQueue* queue, unsigned timeout_ms)
{
    enter_critical_section();

    if (timeout_ms != WAIT_INFINITE) {
        uint64_t now = pit_get_ticks();
        current_thread->sleep_deadline = now + pit_ms_to_ticks(timeout_ms);
        if (current_thread->sleep_deadline == DEADLINE_INFINITE)
            current_thread->sleep_deadline++;
        else if (current_thread->sleep_deadline < now)
            panic("Tick wraparound");
    } else
        current_thread->sleep_deadline = DEADLINE_INFINITE;

    current_thread->state = THREAD_BLOCKED;
    current_thread->waiting_queue = queue;
    STAILQ_INSERT_TAIL(&sleeping_queue, current_thread, threads);
    STAILQ_INSERT_TAIL(queue, current_thread, wait_queue_entry);
    schedule_next();
    current_thread->waiting_queue = NULL;

    struct Thread* t;
    STAILQ_FOREACH(t, queue, wait_queue_entry)
    {
        if (t == current_thread)
            STAILQ_REMOVE(queue, current_thread, Thread, wait_queue_entry);
    }
    leave_critical_section();
}

void queue_notify(struct WaitQueue* queue)
{
    enter_critical_section();
    struct Thread* waiter;
    while ((waiter = STAILQ_FIRST(queue))) {
        waitqueue_remove_thread(queue, waiter);
        if (!waiter->process->exit_requested)
            break;
    }
    leave_critical_section();
}

void queue_notify_all(struct WaitQueue* queue)
{
    enter_critical_section();
    while (!STAILQ_EMPTY(queue))
        queue_notify(queue);
    leave_critical_section();
}

void thread_cancel_wait(struct Thread* thread)
{
    assert(!(read_eflags() & EFLAGS_IF));
    if (!thread->waiting_queue)
        return;
    waitqueue_remove_thread(thread->waiting_queue, thread);
}

