#pragma once

#include <queue.h>
#include <stddef.h>
#include <stdint.h>

#define WAIT_INFINITE UINT32_MAX

enum ThreadState
{
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_TERMINATED,
};

struct Process;
struct Event;

struct Thread
{
    uint32_t esp;
    enum ThreadState state;
    size_t stack_size;
    void* stack;     /* kernel stack */
    void* stack_top; /* kernel stack top */
    char name[16];
    uint64_t wait_started;
    uint64_t wait_duration;
    bool wait_timed;
    struct Process* process;
    struct Event* waiting_event;
    bool wait_signaled;

    STAILQ_ENTRY(Thread) threads;   /* Queues in kthread.c */
    STAILQ_ENTRY(Thread) processes; /* Process thread list in process.c */
};

STAILQ_HEAD(ThreadList, Thread);

/* Binary auto-reset event with at most one blocked waiter */
struct Event
{
    bool pending;
    struct Thread* waiter;
};

typedef void (*thread_entry_t)(void*);

void thread_init(void);

/* If proc is NULL, thread will be created with kernel_task as owning process
 */
struct Thread* thread_create(struct Process* proc, const char* name,
                             thread_entry_t entry, void* arg);
void context_switch(uint32_t* old_esp, uint32_t new_esp);
void thread_yield(void);
void thread_exit(void);
void thread_sleep(uint32_t ticks);
void thread_sleep_ms(uint32_t milliseconds);
void thread_cancel_wait(struct Thread* thread);
void schedule(void);
void scheduler_enqueue(struct Thread* thread);
void thread_dump(void);
struct Event* event_create(void);
void event_free(struct Event* event);

/*
 * Returns false on timeout/cancellation
 * If timeout_ms == 0, returns immediately
 * If timeout_ms == WAIT_INFINITE, blocks indefinitely
 */
bool event_wait(struct Event* event, uint32_t timeout_ms);

void event_signal(struct Event* event);
size_t current_thread_name(char* buf, size_t size);
void thread_get_info(char* buffer, size_t len);
struct Thread* thread_current(void);

