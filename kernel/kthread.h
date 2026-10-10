#pragma once

#include <freebsd/queue.h>
#include <stdint.h>

enum ThreadState
{
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_TERMINATED,
};

struct Thread
{
    uint32_t esp;
    enum ThreadState state;
    void* stack;
    STAILQ_ENTRY(Thread) entry;
};

typedef void (*thread_entry_t)(void*);

struct Thread* thread_create(thread_entry_t entry, void* arg);
void context_switch(uint32_t* old_esp, uint32_t new_esp);
void thread_yield(void);

