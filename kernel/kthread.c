#include "kthread.h"
#include "kernel.h"
#include "kmalloc.h"
#include "trace.h"
#include <assert.h>
#include <string.h>

static STAILQ_HEAD(ThreadList,
                   Thread) ready_queue = STAILQ_HEAD_INITIALIZER(ready_queue);
static STAILQ_HEAD(ThreadList, Thread)
    terminated_queue = STAILQ_HEAD_INITIALIZER(terminated_queue);
static struct Thread* current_thread;
static struct Thread* idle_thread;

void scheduler_enqueue(struct Thread* thread)
{
    thread->state = THREAD_READY;
    STAILQ_INSERT_TAIL(&ready_queue, thread, entry);
}

struct Thread* scheduler_dequeue(void)
{
    struct Thread* next = STAILQ_FIRST(&ready_queue);
    if (next)
        STAILQ_REMOVE_HEAD(&ready_queue, entry);
    else
        next = idle_thread;
    return next;
}

static void panic_thunk(void)
{
    panic("In panic_thunk");
}

/* Create initial thread and makes it the idle thread */
void thread_init()
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

struct Thread* thread_create(thread_entry_t entry, void* arg)
{
    struct Thread* thread = kmalloc(sizeof(struct Thread));
    if (!thread) {
        return NULL;
    }
    memset(thread, 0, sizeof(*thread));
    thread->stack = kmalloc(4096);

    uint32_t* esp = thread->stack + 4096 - sizeof(uint32_t);
    *esp-- = (uintptr_t)arg;
    *esp-- = (uintptr_t)panic_thunk;
    *esp-- = (uintptr_t)entry; /* return address */
    *esp-- = 0;                /* ebp */
    *esp-- = 0;                /* ebx */
    *esp-- = 0;                /* esi */
    *esp-- = 0;                /* edi */
    *esp = read_eflags();
    thread->esp = (uint32_t)(uintptr_t)esp;

    scheduler_enqueue(thread);
    return thread;
}

static void cleanup_terminated()
{
    while (!STAILQ_EMPTY(&terminated_queue)) {
        struct Thread* thread = STAILQ_FIRST(&terminated_queue);
        assert(thread != idle_thread);
        kfree(thread->stack);
        kfree(thread);
    }
}

void thread_yield(void)
{
    if ((read_eflags() & EFLAGS_IF) == 0)
        panic("IF was disabled");

    struct Thread* next = scheduler_dequeue();
    if (next != current_thread) {
        next->state = THREAD_RUNNING;

        struct Thread* old = current_thread;
        current_thread = next;

        scheduler_enqueue(old);
        context_switch(&old->esp, next->esp);
    }

    cleanup_terminated();
}

void thread_exit(void)
{
    if (current_thread == idle_thread)
        panic("Idle thread exited");

    struct Thread* next = scheduler_dequeue();
    assert(next != current_thread)

        next->state = THREAD_RUNNING;
    current_thread->state = THREAD_TERMINATED;
    current_thread = next;

    uint32_t old_esp;
    context_switch(&old_esp, next->esp);

    cleanup_terminated();
}

