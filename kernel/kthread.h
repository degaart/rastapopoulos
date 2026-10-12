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

void thread_init(void);
struct Thread* thread_create(void (*entry)(void*), void* arg);
void context_switch(uint32_t* old_esp, uint32_t new_esp);
void thread_yield(void);
void thread_exit(void);
void schedule(void);
void thread_dump(void);

