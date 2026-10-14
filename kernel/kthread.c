#include "kthread.h"
#include "kernel.h"
#include "kmalloc.h"
#include "pit.h"
#include "trace.h"
#include "tss.h"
#include <assert.h>
#include <string.h>

#define SCHEDULER_QUANTUM 1

static STAILQ_HEAD(ThreadList,
                   Thread) ready_queue = STAILQ_HEAD_INITIALIZER(ready_queue);
static STAILQ_HEAD(ThreadList, Thread)
    terminated_queue = STAILQ_HEAD_INITIALIZER(terminated_queue);
static STAILQ_HEAD(ThreadList, Thread)
    sleeping_queue = STAILQ_HEAD_INITIALIZER(sleeping_queue);
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
    assert((read_eflags() & EFLAGS_IF) == 0);

    uint64_t current_ticks = pit_get_ticks();

    /*
     * STAILQ_REMOVE scans from head, which can have a perf impact when used
     * with STAILQ_FOREACH_SAFE
     * Instead, we scan linearly and use STAILQ_REMOVE_AFTER
     */
    struct Thread* prev = NULL;
    struct Thread* thread = STAILQ_FIRST(&sleeping_queue);
    while (thread) {
        struct Thread* next = STAILQ_NEXT(thread, entry);
        if (thread->sleep_deadline <= current_ticks) {
            if (prev)
                STAILQ_REMOVE_AFTER(&sleeping_queue, prev, entry);
            else
                STAILQ_REMOVE_HEAD(&sleeping_queue, entry);
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
}

static void cleanup_terminated(void);

static void thread_start(void (*entry)(void*), void* arg)
{
    cleanup_terminated();
    entry(arg);
    thread_exit();
}

struct Thread* thread_create(const char* name, thread_entry_t entry, void* arg)
{
    bool const flag = enter_critical_section();
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

        scheduler_ticks = 0;
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
    STAILQ_INSERT_TAIL(&terminated_queue, current_thread, entry);
    current_thread = next;
    scheduler_ticks = 0;

    uint32_t old_esp;
    tss.esp0 = (uintptr_t)next->stack_top;
    context_switch(&old_esp, next->esp);

    panic("Terminated thread resumed");
}

void thread_sleep(uint32_t ticks)
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
        return;
    }

    current_thread->sleep_deadline = pit_get_ticks() + ticks;
    current_thread->state = THREAD_BLOCKED;
    STAILQ_INSERT_TAIL(&sleeping_queue, current_thread, entry);

    struct Thread* next = scheduler_dequeue();
    assert(next != current_thread);

    next->state = THREAD_RUNNING;
    struct Thread* old = current_thread;
    current_thread = next;

    scheduler_ticks = 0;
    tss.esp0 = (uintptr_t)next->stack_top;
    context_switch(&old->esp, next->esp);
    leave_critical_section(iflag);
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

        scheduler_ticks = 0;
        tss.esp0 = (uintptr_t)next->stack_top;
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

size_t current_thread_name(char* buf, size_t size)
{
    bool iflag = enter_critical_section();
    size_t result = strlcpy(buf, current_thread->name, size);
    leave_critical_section(iflag);
    return result;
}

static void strappend(char** buffer, size_t* remaining, char const* text)
{
    while (*remaining && *text) {
        *(*buffer)++ = *text++;
        --*remaining;
    }
    **buffer = '\0';
}

void thread_get_info(char* buffer, size_t len)
{
    if (!len)
        return;

    bool iflag = enter_critical_section();
    size_t remaining = len - 1;
    strappend(&buffer, &remaining, "C:");
    strappend(&buffer, &remaining, current_thread->name);
    strappend(&buffer, &remaining, " R:");

    size_t ready_remaining = remaining > 3 ? remaining - 3 : 0;
    size_t ready_capacity = ready_remaining;
    struct Thread* thread;
    bool first = true;
    STAILQ_FOREACH(thread, &ready_queue, entry)
    {
        if (!ready_remaining)
            break;
        if (!first)
            strappend(&buffer, &ready_remaining, " ");
        strappend(&buffer, &ready_remaining, thread->name);
        first = false;
    }
    remaining -= ready_capacity - ready_remaining;

    strappend(&buffer, &remaining, " T:");
    first = true;
    STAILQ_FOREACH(thread, &terminated_queue, entry)
    {
        if (!remaining)
            break;
        if (!first)
            strappend(&buffer, &remaining, " ");
        strappend(&buffer, &remaining, thread->name);
        first = false;
    }
    leave_critical_section(iflag);
}

