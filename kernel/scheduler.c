#include "scheduler.h"
#include "debug.h"
#include "registers.h"
#include "vmm.h"
#include "gdt.h"
#include "kmalloc.h"
#include "string.h"
#include "pmm.h"
#include "pit.h"

TAILQ_HEAD(tasks, task) _tasks;
TAILQ_HEAD(rtasks, task) _ready_queue;
struct task* _current_task;
struct task* _idle_task;
extern void context_switch(struct task* next);

/*
 * All tasks in _ready_queue must have TASK_STATE_READY as state
 */
static
void check()
{
    struct task* t;
    TAILQ_FOREACH(t, &_ready_queue, rnext) {
        assert2(t->state == TASK_STATE_READY, "task %s state: %d", t->name, t->state);
    }
}

/*
 * Called at when new tasks get cpu time (only one single time)
 */
static TASK_ENTRY
void task_startup()
{
    trace("task_startup");
}

void task_switch(struct task* next)
{
    assert(_current_task != NULL);
    assert(next != _current_task);

    /* Load new pagedir */
    if(next->cr3 != _current_task->cr3) {
        vmm_copy_kernel_mappings(next->pagedir);
        write_cr3(next->cr3);
    }

    /* Set esp0 */
    tss_set_esp0(next->esp0);

    /* Set tasks state */
    if(_current_task->state == TASK_STATE_RUNNING) {        /* Not blocked by task_block() */
        _current_task->state = TASK_STATE_READY;
        TAILQ_INSERT_TAIL(&_ready_queue, _current_task, rnext);
    }

    next->state = TASK_STATE_RUNNING;

    /* 
     * switch stack 
     * Lines after this call will execute only after the new task calls task_switch again
     */
    context_switch(next);
}

/*
 * Chooses another task and switches to it
 */
void schedule()
{
    assert(!interrupts_enabled());
    check();

    if(!TAILQ_EMPTY(&_ready_queue)) {
#if 0
        struct task* t;
        trace("Ready queue:");
        TAILQ_FOREACH(t, &_ready_queue, rnext) {
            trace("\t%s", t->name);
        }
#endif

        struct task* next = TAILQ_FIRST(&_ready_queue);
        TAILQ_REMOVE(&_ready_queue, next, rnext);
        task_switch(next);
    }
}

void task_block(enum task_state state)
{
    assert(state != TASK_STATE_READY && state != TASK_STATE_RUNNING);
    check();

    _current_task->state = state;
    schedule();
}

void task_unblock(struct task* task)
{
    if(TAILQ_EMPTY(&_ready_queue)) {
        task_switch(task);
    } else {
        TAILQ_INSERT_TAIL(&_ready_queue, task, rnext);
    }
}

struct task* task_create(const char* name, void(*entry)())
{
    check();

    struct task* task = kmalloc(sizeof(struct task));
    bzero(task, sizeof(struct task));
    strlcpy(task->name, name, sizeof(task->name));
    task->stack = kmalloc_aligned(PAGE_SIZE, 16);

    uint32_t* stack = (uint32_t*)task->stack;

    /* TODO: Propertly align the stack */
    stack[1023] = (uint32_t)entry;
    stack[1022] = (uint32_t)task_startup;
    task->esp = (unsigned char*)&stack[1021];
    task->esp0 = (unsigned char*)task->stack + PAGE_SIZE;
    task->pagedir = vmm_create_pagedir();
    task->cr3 = vmm_get_frame(task->pagedir);

    TAILQ_INSERT_TAIL(&_tasks, task, tnext);

    task->state = TASK_STATE_READY;
    TAILQ_INSERT_TAIL(&_ready_queue, task, rnext);

    return task;
}

static
void scheduler_timer(void* unused)
{
    check();
    trace("Scheduler timer");
}

void scheduler_init()
{
    TAILQ_INIT(&_tasks);
    TAILQ_INIT(&_ready_queue);

    /* Create initial task */
    _current_task = kmalloc(sizeof(struct task));
    bzero(_current_task, sizeof(struct task));
    strlcpy(_current_task->name, "task1", sizeof(_current_task->name));
    _current_task->esp = 0;          /* will be filled by task_switch */
    _current_task->stack = kmalloc_aligned(PAGE_SIZE, 16);
    _current_task->esp0 = (unsigned char*)_current_task->stack + PAGE_SIZE;
    _current_task->pagedir = vmm_current_pagedir();
    _current_task->cr3 = vmm_get_frame(_current_task->pagedir);
    _current_task->state = TASK_STATE_RUNNING;
    TAILQ_INSERT_TAIL(&_tasks, _current_task, tnext);

    pit_add_timer(18, scheduler_timer, NULL);
}

/************************************************************************************************
 * TESTS                                                                                        *
 ************************************************************************************************/
static int counter;

static
void task2_entry()
{
    while(1) {
        trace("task2 running");
        //schedule();
        task_block(TASK_STATE_PAUSED);
    }
}

static
void task1_entry()
{
    struct task* task2 = task_create("task2", task2_entry);
    while(1) {
        trace("task1 running");
        counter++;
        schedule();
        if(((counter % 3) == 0) && task2->state == TASK_STATE_PAUSED)
            task_unblock(task2);
    }
}

void test_scheduler()
{
    trace(" *** Testing scheduler ***");
    scheduler_init();
    task1_entry();
}




