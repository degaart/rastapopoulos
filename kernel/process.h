#pragma once

#include "address_space.h"
#include "kthread.h"
#include <freebsd/queue.h>

typedef uint8_t pid_t;

enum ProcessState
{
    PROCESS_CONSTRUCTING,
    PROCESS_ALIVE,
    PROCESS_EXITING,
    PROCESS_EXITED,
};

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

    TAILQ_ENTRY(Process) processes;
    TAILQ_ENTRY(Process) exited_processes;
};
TAILQ_HEAD(ProcessList, Process);

void process_init(void);
struct Process* process_current(void);
struct Process* process_create(const char* name);
void process_destroy(struct Process* proc);
void process_add_thread(struct Process* proc, struct Thread* thread);
void process_remove_thread(struct Process* proc, struct Thread* thread);
void process_set_current(struct Process* proc);
void process_exit(uint8_t status) __attribute__((noreturn));
void process_cleanup_exited(void);

