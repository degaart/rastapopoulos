#include "scheduler.h"
#include "debug.h"
#include "registers.h"
#include "vmm.h"
#include "gdt.h"
#include "kmalloc.h"
#include "string.h"
#include "pmm.h"
#include "pit.h"

TAILQ_HEAD(task_list, task);
struct task_list _tasks;
struct task_list _ready_queue;
struct task_list _sleeping_tasks;

struct task* _current_task;
static struct task* _idle_task;
static uint64_t _time_slice_remaining;
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

    _time_slice_remaining = 10;

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

void task_unblock(struct task* task, bool immediate)
{
    if(immediate && TAILQ_EMPTY(&_ready_queue)) {
        task_switch(task);
    } else {
        task->state = TASK_STATE_READY;
        TAILQ_INSERT_TAIL(&_ready_queue, task, rnext);
    }
}

void msleep_until(uint64_t deadline)
{
    if(deadline < pit_get_clock())
        return;

    _current_task->deadline = deadline;
    TAILQ_INSERT_HEAD(&_sleeping_tasks, _current_task, snext);
    task_block(TASK_STATE_SLEEPING);
}

void msleep(uint64_t ms)
{
    msleep_until(pit_get_clock() + ms);
}

void sleep(int secs)
{
    msleep((uint64_t)secs * 1000LL);
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
void idle_task_entry()
{
    while(1) {
        sti();
        hlt();
    }
}

static
void scheduler_timer(void* unused)
{
    check();

    uint64_t now = pit_get_clock();
    struct task* task, *tmp;
    TAILQ_FOREACH_SAFE(task, &_sleeping_tasks, snext, tmp) {
        if(task->state == TASK_STATE_SLEEPING && task->deadline <= now) {
            TAILQ_REMOVE(&_sleeping_tasks, task, snext);
            task_unblock(task, false);
        }
    }

    if(_time_slice_remaining <= pit_tick_length()) {
        schedule();
    } else {
        _time_slice_remaining -= pit_tick_length();
    }
}

void scheduler_init()
{
    TAILQ_INIT(&_tasks);
    TAILQ_INIT(&_ready_queue);
    TAILQ_INIT(&_sleeping_tasks);

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

    _idle_task = task_create("IDLE_TASK", idle_task_entry);

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
        sti();
        trace("task2 running");
    }
}

static
void task1_entry()
{
    task_create("task2", task2_entry);
    while(1) {
        sti();
        trace("task1 running");
    }
}

void test_scheduler()
{
    trace(" *** Testing scheduler ***");
    scheduler_init();
    task1_entry();
}




