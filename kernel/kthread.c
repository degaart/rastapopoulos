#include "kthread.h"
#include "kernel.h"
#include "kmalloc.h"
#include <string.h>

static STAILQ_HEAD(ThreadList,
                   Thread) ready_queue = STAILQ_HEAD_INITIALIZER(ready_queue);
static struct Thread* current_thread;

void scheduler_enqueue(struct Thread* thread)
{
    thread->state = THREAD_READY;
    STAILQ_INSERT_TAIL(&ready_queue, thread, entry);
}

static void panic_thunk(void)
{
    panic("In panic_thunk");
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
    *esp-- = read_eflags();    /* IOPL=0 PF */
    *esp-- = 0;                /* ebp */
    *esp-- = 0;                /* ebx */
    *esp-- = 0;                /* esi */
    *esp = 0;                  /* edi */
    thread->esp = (uint32_t)(uintptr_t)esp;

    scheduler_enqueue(thread);
    return thread;
}

void thread_yield(void)
{
    struct Thread* next = STAILQ_FIRST(&ready_queue);
    if (!next) {
        panic("No runnable threads");
    }

    STAILQ_REMOVE_HEAD(&ready_queue, entry);
    next->state = THREAD_RUNNING;

    struct Thread* old = current_thread;
    current_thread = next;

    if (old) {
        scheduler_enqueue(old);
        context_switch(&old->esp, next->esp);
    } else {
        uint32_t old_esp;
        context_switch(&old_esp, next->esp);
    }
}

