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
    assert(thread != idle_thread);

    bool flag = enter_critical_section();
    thread->state = THREAD_READY;
    STAILQ_INSERT_TAIL(&ready_queue, thread, threads);
    leave_critical_section(flag);
}

struct Thread* scheduler_dequeue(void)
{
    bool flag = enter_critical_section();
    struct Thread* next = STAILQ_FIRST(&ready_queue);
    if (next)
        STAILQ_REMOVE_HEAD(&ready_queue, threads);
    else
        next = idle_thread;
    leave_critical_section(flag);
    return next;
}

static void finish_wait(struct Thread* thread, bool signaled)
{
    assert(thread->state == THREAD_BLOCKED);
    STAILQ_REMOVE(&sleeping_queue, thread, Thread, threads);
    if (thread->waiting_event) {
        thread->waiting_event->waiter = NULL;
        thread->waiting_event = NULL;
    }
    thread->wait_timed = false;
    thread->wait_signaled = signaled;
    scheduler_enqueue(thread);
}

void thread_cancel_wait(struct Thread* thread)
{
    bool flag = enter_critical_section();
    if (thread->state == THREAD_BLOCKED)
        finish_wait(thread, false);
    leave_critical_section(flag);
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
            (thread->wait_timed &&
             current_ticks - thread->wait_started >= thread->wait_duration)) {
            if (prev)
                STAILQ_REMOVE_AFTER(&sleeping_queue, prev, threads);
            else
                STAILQ_REMOVE_HEAD(&sleeping_queue, threads);
            if (thread->waiting_event) {
                thread->waiting_event->waiter = NULL;
                thread->waiting_event = NULL;
            }
            thread->wait_timed = false;
            thread->wait_signaled = false;
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
    if (proc->state != PROCESS_ALIVE)
        return false;

    bool const flag = enter_critical_section();
    if (proc->state != PROCESS_ALIVE) {
        leave_critical_section(flag);
        return NULL;
    }

    struct Thread* thread = kmalloc(sizeof(struct Thread));
    if (!thread) {
        leave_critical_section(flag);
        return NULL;
    }

    memset(thread, 0, sizeof(*thread));
    strlcpy(thread->name, name, sizeof(thread->name));

    thread->stack_size = 4096;
    thread->stack = kmalloc(thread->stack_size);
    if (!thread->stack) {
        kfree(thread);
        leave_critical_section(flag);
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
    leave_critical_section(flag);
    return thread;
}

static void cleanup_terminated(void)
{
    bool flag = enter_critical_section();
    while (!STAILQ_EMPTY(&terminated_queue)) {
        struct Thread* thread = STAILQ_FIRST(&terminated_queue);
        trace("Destroying thread \"%s\"", thread->name);

        STAILQ_REMOVE_HEAD(&terminated_queue, threads);
        assert(thread != idle_thread);

        process_remove_thread(thread->process, thread);
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
        scheduler_ticks = 0;

        process_set_current(next->process);
        tss.esp0 = (uintptr_t)next->stack_top;
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
    STAILQ_INSERT_TAIL(&terminated_queue, current_thread, threads);
    current_thread = next;
    scheduler_ticks = 0;

    process_set_current(next->process);
    uint32_t old_esp;
    tss.esp0 = (uintptr_t)next->stack_top;
    context_switch(&old_esp, next->esp);
    panic("Unreachable code");
}

static void sleep_ticks(uint64_t ticks)
{
    assert(current_thread != idle_thread);

    bool iflag = enter_critical_section();
    if (ticks == 0) {
        if (!STAILQ_EMPTY(&ready_queue)) {
            /* if another thread is ready, switch to it, otherwise return
             * immediately */
            thread_yield();
        }
        leave_critical_section(iflag);
        if (current_thread->process->exit_requested)
            thread_exit();
        return;
    }

    current_thread->wait_started = pit_get_ticks();
    current_thread->wait_duration = ticks;
    current_thread->wait_timed = true;
    current_thread->state = THREAD_BLOCKED;
    STAILQ_INSERT_TAIL(&sleeping_queue, current_thread, threads);

    struct Thread* next = scheduler_dequeue();
    assert(next != current_thread);

    next->state = THREAD_RUNNING;
    struct Thread* old = current_thread;
    current_thread = next;

    scheduler_ticks = 0;
    process_set_current(next->process);
    tss.esp0 = (uintptr_t)next->stack_top;
    context_switch(&old->esp, next->esp);
    leave_critical_section(iflag);

    if (current_thread->process->exit_requested)
        thread_exit();
}

void thread_sleep(uint32_t ticks)
{
    sleep_ticks(ticks);
}

void thread_sleep_ms(uint32_t milliseconds)
{
    sleep_ticks(pit_ms_to_ticks(milliseconds));
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
    if (event)
        memset(event, 0, sizeof(*event));
    return event;
}

void event_free(struct Event* event)
{
    kfree(event);
}

bool event_wait(struct Event* event, uint32_t timeout_ms)
{
    bool iflag = enter_critical_section();
    if (current_thread->process->exit_requested) {
        leave_critical_section(iflag);
        return false;
    }
    if (event->pending) {
        event->pending = false;
        leave_critical_section(iflag);
        return true;
    }
    if (timeout_ms == 0) {
        leave_critical_section(iflag);
        return false;
    }
    assert(current_thread != idle_thread);
    assert(event->waiter == NULL);
    struct Thread* old = current_thread;
    old->waiting_event = event;
    old->wait_signaled = false;
    event->waiter = old;
    old->wait_started = pit_get_ticks();
    old->wait_timed = timeout_ms != WAIT_INFINITE;
    old->wait_duration = old->wait_timed ? pit_ms_to_ticks(timeout_ms) : 0;
    old->state = THREAD_BLOCKED;
    STAILQ_INSERT_TAIL(&sleeping_queue, old, threads);

    struct Thread* next = scheduler_dequeue();
    assert(next != old);
    next->state = THREAD_RUNNING;
    current_thread = next;
    scheduler_ticks = 0;
    process_set_current(next->process);
    tss.esp0 = (uintptr_t)next->stack_top;
    context_switch(&old->esp, next->esp);
    bool result = old->wait_signaled && !old->process->exit_requested;
    leave_critical_section(iflag);
    return result;
}

void event_signal(struct Event* event)
{
    bool iflag = enter_critical_section();
    if (event->waiter) {
        struct Thread* waiter = event->waiter;
        bool canceled = waiter->process->exit_requested;
        finish_wait(waiter, !canceled);
        if (canceled)
            event->pending = true;
    } else {
        event->pending = true;
    }
    leave_critical_section(iflag);
}

size_t current_thread_name(char* buf, size_t size)
{
    bool iflag = enter_critical_section();
    size_t result = strlcpy(buf, current_thread->name, size);
    leave_critical_section(iflag);
    return result;
}

void thread_get_info(char* buffer, size_t len)
{
    if (!len)
        return;

    bool iflag = enter_critical_section();
    snprintf(buffer, len, "CT: %s",
             current_thread ? current_thread->name : "NULL");
    leave_critical_section(iflag);
}

struct Thread* thread_current(void)
{
    return current_thread;
}

