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

struct Event
{
    bool pending;
    struct Thread* waiter;
};

typedef void (*thread_entry_t)(void*);

void thread_init(void);
struct Thread* thread_create(thread_entry_t entry, void* arg);
void context_switch(uint32_t* old_esp, uint32_t new_esp);
void thread_yield(void);
void thread_exit(void);
void schedule(void);
void thread_dump(void);
struct Event* event_create(void);
void event_free(struct Event* event);
void event_wait(struct Event* event);
void event_signal(struct Event* event);

