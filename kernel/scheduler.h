#pragma once

/*
 * Cappy scheduler implementation
 * We run all kernel code with interrupts disabled to simplify things
 * Except in the idle task were we should be preempted
 *
 * The low-level code to switch tasks happens in context_switch(), which just
 * switches esp and then returns, effectively switching to a new thread
 *
 * Then task_switch() does the necessary things to switch to a new kernel task
 *
 * When we jump to usermode, we set the IF flag to allow for task switches to occur
 *
 * _tasks contains all tasks
 * _ready_queue contains all tasks that are ready to run.
 *      tasks in _ready_queue must have state be TASK_STATE_READY
 * _current_task contains the current task.
 *      _current_task->state must be TASK_STATE_RUNNING.
 *      _current_task must not be listed in any other list except _tasks
 *
 * schedule() wakes tasks as necessary, then calls task_switch()
 *
 * task_block() blocks a task
 * task_unblock() puts a task into the ready queue
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "queue.h"

enum task_state {
    TASK_STATE_READY,               /* Ready to run, but is not running */
    TASK_STATE_RUNNING,             /* Currently running */
    TASK_STATE_PAUSED,              /* Blocked until another task unblocks it */
    TASK_STATE_SLEEPING,
};

/*
 * task control block
 * esp must be first as it's accessed by context_switch()
 */
struct task {
    unsigned char* esp;

    unsigned long cr3;
    uint32_t* pagedir;
    void* esp0;
    void* stack;
    enum task_state state;
    char name[64];
    uint64_t deadline;          /* if task is sleeping, when to unblock it */

    TAILQ_ENTRY(task) rnext;    /* node for ready_tasks */
    TAILQ_ENTRY(task) tnext;    /* node for tasks */
    TAILQ_ENTRY(task) snext;    /* not for sleeping_tasks */
};

void scheduler_init();
void task_switch(struct task* next);
void schedule();
struct task* task_create(const char* name, void(*entry)());
void test_scheduler();

#define TASK_ENTRY __attribute__((force_align_arg_pointer))

