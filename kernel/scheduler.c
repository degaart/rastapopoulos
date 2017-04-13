#include "scheduler.h"
#include "locks.h"
#include "debug.h"
#include "timer.h"
#include "syscall_handler.h"
#include "syscall.h"
#include "ipc.h"
#include "idt.h"
#include "gdt.h"
#include "kmalloc.h"
#include "pmm.h"
#include "string.h"
#include "registers.h"
#include "initrd.h"
#include "elf.h"
#include "task_info.h"

static struct task_list ready_queue = {0};
static spinlock_t ready_queue_lock = SPINLOCK_INIT;

static struct task_list sleeping_queue = {0};
static spinlock_t sleeping_queue_lock = SPINLOCK_INIT;

static struct task_list msgwait_queue = {0};
static spinlock_t msgwait_queue_lock = SPINLOCK_INIT;

static struct task_list exited_queue = {0};
static spinlock_t exited_queue_lock = SPINLOCK_INIT;

static struct task* current_task = NULL;
static struct task* idle_task = NULL;

static int next_pid_value = 0;

static void save_task_state(struct task* task, const struct isr_regs* regs)
{
    /*
     * Save current process context
     * From ring0: useresp is invalid
     */
    unsigned descriptor_mask = ~0x3;
    if((regs->cs & descriptor_mask) == KERNEL_CODE_SEG) {
        task->context.esp = regs->esp + (5 * sizeof(uint32_t));
    } else if((regs->cs & descriptor_mask) == USER_CODE_SEG) {
        task->context.esp = regs->useresp;
    } else {
        panic("Invalid cs value: %p", regs->cs);
    }

    task->context.cs = regs->cs;
    task->context.ds = task->context.ss = regs->ds;
    task->context.eflags = regs->eflags;
    task->context.eip = regs->eip;
    task->context.edi = regs->edi;
    task->context.esi = regs->esi;
    task->context.edx = regs->edx;
    task->context.ecx = regs->ecx;
    task->context.ebx = regs->ebx;
    task->context.eax = regs->eax;
    task->context.ebp = regs->ebp;
}

void save_current_task_state(const struct isr_regs* regs)
{
    assert(current_task);
    save_task_state(current_task, regs);
}

/* Switch to specified task */
static void task_switch(struct task* task)
{
    assert(task);
    
    cli();
    current_task = task;

    vmm_copy_kernel_mappings(task->pagedir);
    tss_set_kernel_stack(KERNEL_STACK + PAGE_SIZE);
    switch_context(&task->context);

    panic("Invalid code path");
}

/* Switch to next ready task, or idle task */
static void task_switch_next()
{
    struct task* next_task = NULL;

    /* Awake sleeping tasks whose deadline has arrived */
    uint64_t now = timer_timestamp();

    checked_lock(&sleeping_queue_lock);
    list_foreach(task, task, &sleeping_queue, node) {
        if(now >= task->sleep_deadline) {
            next_task = task;
            list_remove(&sleeping_queue, task, node);
            break;
        }
    }
    checked_unlock(&sleeping_queue_lock);

    if(!next_task) {
        /* No awoken tasks, get task from ready queue */
        checked_lock(&ready_queue_lock);
        next_task = list_head(&ready_queue);
        if(next_task) {
            list_remove(&ready_queue, next_task, node);
        }
        checked_unlock(&ready_queue_lock);
    }

    if(!next_task) {
        /* Else run idle task */
        next_task = idle_task;
    }

    task_switch(next_task);
}


static void scheduler_timer(void* data, const struct isr_regs* regs)
{
    /* Save current task state */
    assert(current_task);
    save_task_state(current_task, regs);

    /* Push current task into ready queue */
    if(current_task != idle_task) {
        checked_lock(&ready_queue_lock);
        list_append(&ready_queue, current_task, node);
        checked_unlock(&ready_queue_lock);
    }

    /* Collect exited tasks */
    checked_lock(&exited_queue_lock);
    list_foreach(task, task, &exited_queue, node) {
        trace("Collecting task %s (%d)", task->name, task->pid);

        list_remove(&exited_queue, task, node);
        vmm_destroy_pagedir(task->pagedir);
        kfree(task);
    }
    checked_unlock(&exited_queue_lock);

    /* If no more tasks to run, reboot */
    bool moretasks = true;

    checked_lock(&ready_queue_lock);
    checked_lock(&sleeping_queue_lock);
    checked_lock(&msgwait_queue_lock);
    if(list_empty(&ready_queue) &&
       list_empty(&sleeping_queue) &&
       list_empty(&msgwait_queue)) {
        moretasks = false;
    } else if(list_empty(&ready_queue) &&
              list_empty(&sleeping_queue) &&
              !list_empty(&msgwait_queue)) {
        if(list_head(&msgwait_queue) == list_tail(&msgwait_queue)) {
            /*
             * This task is not magically gonna awake itself
             * (normally, this is the case for the logger, which should
             * be the last task alive in the system)
             */
            moretasks = false;
        }
    }
    checked_unlock(&msgwait_queue_lock);
    checked_unlock(&sleeping_queue_lock);
    checked_unlock(&ready_queue_lock);

    if(!moretasks) {
        trace("No more tasks to run. Rebooting");
        reboot();
    }

    /* Switch to next task */
    task_switch_next();
    invalid_code_path();
}

/*
 * Create a new task structure (but does not push it to any queues)
 */
static struct task* task_create(const char* name)
{
    struct task* result = kmalloc(sizeof(struct task));
    bzero(result, sizeof(struct task));

    enter_critical_section();
    result->pid = next_pid_value++;
    leave_critical_section();

    strlcpy(result->name, name, sizeof(result->name));
    result->pagedir = vmm_clone_pagedir();

    return result;
}

/*
 * Get a task by it's pid
 */
struct task* task_get(int pid)
{
    struct task* result = {0};

    enter_critical_section();

    if(pid == idle_task->pid)
        result = idle_task;

    if(!result) {
        if(pid == current_task->pid)
            result = current_task;
    }

    if(!result) {
        checked_lock(&ready_queue_lock);

        list_foreach(task, task, &ready_queue, node) {
            if(task->pid == pid) {
                result = task;
                break;
            }
        }
        checked_unlock(&ready_queue_lock);
    }

    if(!result) {
        checked_lock(&sleeping_queue_lock);
        list_foreach(task, task, &sleeping_queue, node) {
            if(task->pid == pid) {
                result = task;
                break;
            }
        }
        checked_unlock(&sleeping_queue_lock);
    }

    if(!result) {
        checked_lock(&msgwait_queue_lock);
        list_foreach(task, task, &msgwait_queue, node) {
            if(task->pid == pid) {
                result = task;
                break;
            }
        }
        checked_unlock(&msgwait_queue_lock);
    }

    leave_critical_section();

    return result;
}

void task_wait_message(int port_number, const struct isr_regs* regs)
{
    save_task_state(current_task, regs);
    current_task->wait_port = port_number;

    checked_lock(&msgwait_queue_lock);
    list_append(&msgwait_queue, current_task, node);
    checked_unlock(&msgwait_queue_lock);

    /* Switch to next task */
    task_switch_next();
    invalid_code_path();
}

void wake_tasks_for_port(int port_number)
{
    /* Wake any process waiting on this port */
    checked_lock(&msgwait_queue_lock);
    list_foreach(task, task, &msgwait_queue, node) {
        if(task->wait_port == port_number) {
            list_remove(&msgwait_queue, task, node);

            checked_lock(&ready_queue_lock);
            list_append(&ready_queue, task, node);
            checked_unlock(&ready_queue_lock);
            kernel_heap_check();
        }
    }
    checked_unlock(&msgwait_queue_lock);
}

const char* current_task_name()
{
    return current_task ? current_task->name : NULL;
}

void current_task_set_name(const char* name)
{
    strlcpy(current_task->name, name, sizeof(current_task->name));
}

int current_task_pid()
{
    return current_task ? current_task->pid : -1;
}

/*
 * Put current task into tail of ready queue and switch to next ready task
 */
static uint32_t syscall_yield_handler(struct isr_regs* regs)
{
    save_task_state(current_task, regs);

    checked_lock(&ready_queue_lock);
    list_append(&ready_queue, current_task, node);
    checked_unlock(&ready_queue_lock);

    task_switch_next();
    invalid_code_path();
    return 0;
}

/* 
 * Fork current process
 * Returns:
 *  Parent process:         pid of child process
 *  Child process:          0
 */
static uint32_t syscall_fork_handler(struct isr_regs* regs)
{
    volatile int result = 0;

    struct task* new_task = task_create(current_task->name);
    save_task_state(new_task, regs);
    new_task->context.cr3 = vmm_get_physical(new_task->pagedir);
    new_task->context.eax = 0;

    enter_critical_section();
    checked_lock(&ready_queue_lock);
    list_append(&ready_queue, new_task, node);
    checked_unlock(&ready_queue_lock);
    leave_critical_section();

    result = new_task->pid;

    return result;
}

static uint32_t syscall_setname_handler(struct isr_regs* regs)
{
    const char* new_name = (const char*)regs->ebx;
    current_task_set_name(new_name);
    return 0;
}

static uint32_t syscall_exit_handler(struct isr_regs* regs)
{
    /* Put into exited queue, will be collected next time scheduler runs */
    checked_lock(&exited_queue_lock);
    list_append(&exited_queue, current_task, node);
    checked_unlock(&exited_queue_lock);

    task_switch_next();
    invalid_code_path();
}

static uint32_t syscall_sleep_handler(struct isr_regs* regs)
{
    unsigned millis = regs->ebx;

    save_task_state(current_task, regs);
    current_task->sleep_deadline = timer_timestamp() + millis;
    
    checked_lock(&sleeping_queue_lock);
    list_append(&sleeping_queue, current_task, node);
    checked_unlock(&sleeping_queue_lock);

    task_switch_next();
    invalid_code_path();
}

static uint32_t syscall_reboot_handler(struct isr_regs* regs)
{
    trace("Reboot requested");
    reboot();
}

static uint32_t syscall_exec_handler(struct isr_regs* regs)
{
    const char* filename = (const char*)regs->ebx;
    assert(filename);
    assert(*filename);

    trace("(%d %s) exec: %s", 
          current_task_pid(), 
          current_task_name(), 
          filename);

    /* Load file from initrd */
    const struct initrd_file* file = initrd_get_file(filename);
    if(!file) {
        trace("Failed to load %s", filename);
        return 0;
    }

    /* Unmap process memory */
    for(unsigned char* va = (unsigned char*)USER_START;
        va < USER_STACK; 
        va += PAGE_SIZE) {

        assert(va != USER_STACK);
        uint32_t pa = vmm_get_physical(va);
        if(pa) {
            vmm_unmap(va);
            pmm_free(pa);
        }
    }

    /* Load elf file */
    elf_entry_t entry = load_elf(file->data, file->size);

    /* Reset process state */
    current_task_set_name(filename);
    regs->esp = (uint32_t)(USER_STACK + PAGE_SIZE);
    regs->useresp = (uint32_t)(USER_STACK + PAGE_SIZE);
    regs->eflags = read_eflags() | EFLAGS_IF;
    regs->eip = (uint32_t)entry;

    return 0;
}

/*
 * Get information about a task, given its pid
 * Params
 *  ebx     pid
 *  ecx     struct task_info*
 * Returns
 *  0       failure
 * !=0      success
 */
static uint32_t syscall_task_info_handler(struct isr_regs* regs)
{
    uint32_t result = 0;

    int pid = regs->ebx;
    struct task_info* buffer = (struct task_info*)regs->ecx;

    struct task* task = task_get(pid);
    if(task) {
        buffer->pid = pid;
        strlcpy(buffer->name, task->name, sizeof(buffer->name));
        result = 1;
    }

    return result;
}

/*
 * transform current task into an user task
 */
void jump_to_usermode()
{
    uint32_t user_entry = read_ebx();

    struct task* task = current_task;

    task->context.cs = USER_CODE_SEG | RPL3;
    task->context.ds = task->context.ss = USER_DATA_SEG | RPL3;

    vmm_map(USER_STACK, pmm_alloc(), VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE | VMM_PAGE_USER);
    memset(USER_STACK, 0xCC, PAGE_SIZE);

    task->context.esp = (uint32_t)(USER_STACK + PAGE_SIZE);

    task->context.eflags |= EFLAGS_IF;
    task->context.eip = (uint32_t)user_entry;

    switch_context(&task->context);
    invalid_code_path();
}

static void idle_task_entry()
{
    while(true) {
        hlt();
    }
}

void scheduler_start(void (*user_entry)())
{
    /* Init global data */
    list_init(&ready_queue);
    list_init(&sleeping_queue);
    list_init(&msgwait_queue);
    list_init(&exited_queue);

    /* Install scheduler timer */
    timer_schedule(scheduler_timer, NULL, 50, true);


    /* Install syscalls */
    syscall_register(SYSCALL_YIELD, syscall_yield_handler);
    syscall_register(SYSCALL_FORK, syscall_fork_handler);
    syscall_register(SYSCALL_SETNAME, syscall_setname_handler);
    syscall_register(SYSCALL_EXIT, syscall_exit_handler);
    syscall_register(SYSCALL_SLEEP, syscall_sleep_handler);
    syscall_register(SYSCALL_REBOOT, syscall_reboot_handler);
    syscall_register(SYSCALL_EXEC, syscall_exec_handler);
    syscall_register(SYSCALL_TASK_INFO, syscall_task_info_handler);

    /* Map kernel stack */ 
    uint32_t stack_frame = pmm_alloc();
    vmm_map(KERNEL_STACK, stack_frame, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
    memset(KERNEL_STACK, 0xCC, PAGE_SIZE);

    tss_set_kernel_stack(KERNEL_STACK + PAGE_SIZE);

    /* Create first task (init) */
    struct task* task = task_create("init");
    task->context.cs = KERNEL_CODE_SEG;
    task->context.ds = KERNEL_DATA_SEG;
    task->context.ss = KERNEL_DATA_SEG;
    task->context.cr3 = vmm_get_physical(task->pagedir);
    task->context.esp = (uint32_t)(KERNEL_STACK + PAGE_SIZE);
    task->context.eflags = read_eflags() | EFLAGS_IF;
    task->context.eip = (uint32_t)jump_to_usermode;
    task->context.ebx = (uint32_t)user_entry;

    /* Create idle_task */
    struct task* task1 = task_create("idle_task");
    task1->context.cs = KERNEL_CODE_SEG;
    task1->context.ds = task1->context.ss = KERNEL_DATA_SEG;
    task1->context.cr3 = vmm_get_physical(task1->pagedir);
    task1->context.esp = (uint32_t)(KERNEL_STACK + PAGE_SIZE);
    task1->context.eflags = read_eflags() | EFLAGS_IF;
    task1->context.eip = (uint32_t)idle_task_entry;
    idle_task = task1;

    /* Switch to first task */
    task_switch(task);
    invalid_code_path();
}

