#pragma once

#include "address_space.h"
#include "kthread.h"
#include "refcount.h"
#include <queue.h>

typedef uint8_t pid_t;

enum ProcessState
{
    PROCESS_CONSTRUCTING,
    PROCESS_ALIVE,
    PROCESS_EXITING,
    PROCESS_EXITED,
};

struct KeyEventQueue;
struct KeyEvent;

struct Process
{
    pid_t pid;
    char* name;
    struct AddressSpace* as;
    enum ProcessState state;
    struct ThreadList threads;
    size_t live_threads;
    uint8_t exit_status;
    bool exit_requested;
    struct Process* parent;
    struct KeyEventQueue* input_queue;
    bool input_queue_overflow;
    struct Event input_event;
    struct Event termination_event;
    REFCOUNT();

    TAILQ_ENTRY(Process) processes;
    TAILQ_ENTRY(Process) exited_processes;
};
TAILQ_HEAD(ProcessList, Process);
REFCOUNT_DECLARE(process, Process);

void process_init(void);
struct Process* process_current(void);
struct Process* process_create(const char* name);
void process_add_thread(struct Process* proc, struct Thread* thread);
void process_remove_thread(struct Process* proc, struct Thread* thread);
void process_set_current(struct Process* proc);
void process_exit(uint8_t status) __attribute__((noreturn));
void process_cleanup_exited(void);
void process_push_input_event(const struct KeyEvent* event);
bool process_get_input_event(struct KeyEvent* event, bool* overflow);
bool process_peek_input_event(struct KeyEvent* event, bool* overflow);
void process_deactivate(struct Process* process);
void process_daemonize(struct Process* process);
void process_get_info(char* buffer, size_t len);

