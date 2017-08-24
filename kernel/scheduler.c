#include "scheduler.h"
#include "locks.h"
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
#include "kdebug.h"
#include "util.h"
#include "kernel_task.h"

/************************************************************************************
 * Task state structure
 ************************************************************************************/
struct task {
    list_declare_node(task) node;
    int pid;
    char name[TASK_NAME_MAX];
    struct pagedir* pagedir;
    struct context context;

    /* Waking condition */
    int wait_canrecv_port;          /* Wait until port has a message to receive */
    int wait_cansend_port;          /* Wait until port is open and can receive messages */
    uint64_t sleep_deadline;
};
list_declare(task_list, task);

/************************************************************************************
 * queues
 ************************************************************************************/
/* Tasks ready to be run */
static struct task_list ready_queue = {0};

/* Tasks sleeping until a condition is met */
static struct task_list sleeping_queue = {0};

/* Exited tasks waiting to be collected */
static struct task_list exited_queue = {0};

/************************************************************************************
 * declarations
 ************************************************************************************/
static int next_pid_value = 0;
static struct task* current_task = NULL;
static struct task* idle_task = NULL;

static void scheduler_perform_checks();

/************************************************************************************
 * Implementation
 ************************************************************************************/

/*
 * Save task state (regs) into task structure
 */
static void save_task_state(struct task* task, const struct isr_regs* regs)
{
    /*
     * Save current process context
     * When task is resumed from save state, control resumes
     * after the interrupt call
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

/*
 * Save current task state
 * Should only be called from syscall_handler()
 */
void save_current_task_state(const struct isr_regs* regs)
{
    assert(current_task);
    assert(!interrupts_enabled());

    save_task_state(current_task, regs);
}

/* Switch to specified task */
static void task_switch(struct task* task)
{
    assert(task);
    assert(!interrupts_enabled());

    //trace("Switching to task %s", task->name);
    
    current_task = task;

    scheduler_perform_checks();

    vmm_copy_kernel_mappings(task->pagedir);
    tss_set_kernel_stack(KERNEL_STACK + PAGE_SIZE);
    switch_context(&task->context);

    panic("Invalid code path");
}

/* 
 * Switch to next ready task, or idle task
 * Careful when calling this function. Take into account the fact
 * that the caller's state must be saved so we can return to it
 * after our process is resumed
 */
static void task_switch_next()
{
    struct task* next_task = NULL;

    /* Awake sleeping tasks whose deadline has arrived */
    uint64_t now = timer_timestamp();

    list_foreach(task, task, &sleeping_queue, node) {
        if(task->sleep_deadline && now >= task->sleep_deadline) {
            next_task = task;
            list_remove(&sleeping_queue, task, node);
            break;
        }
    }

    if(!next_task) {
        /* No awoken tasks, get task from ready queue */
        next_task = list_head(&ready_queue);
        if(next_task) {
            list_remove(&ready_queue, next_task, node);
        }
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

    scheduler_perform_checks();

    /* Push current task into ready queue */
    if(current_task != idle_task) {
        //trace("Pushing %s into ready queue", current_task->name);
        list_append(&ready_queue, current_task, node);
    }

    /* Collect exited tasks */
    list_foreach(task, task, &exited_queue, node) {
        trace("Collecting task %s (%d)", task->name, task->pid);

        list_remove(&exited_queue, task, node);
        vmm_destroy_pagedir(task->pagedir);
        kfree(task);
    }

    /* If no more tasks to run, reboot */
    bool moretasks = true;

    if(list_empty(&ready_queue) &&
       list_empty(&sleeping_queue)) {
        moretasks = false;
    } else if(list_empty(&ready_queue)) {
        bool may_wakeup = false;
        list_foreach(task, task, &sleeping_queue, node) {
            if(task->sleep_deadline != 0) {
                may_wakeup = false;
                break;
            }
        }
        if(!may_wakeup)
            moretasks = false;
    }

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

    result->pid = next_pid_value++;
    assert(result->pid < 64);

    strlcpy(result->name, name, sizeof(result->name));
    result->pagedir = vmm_clone_pagedir();

    return result;
}

/*
 * Get a task by it's pid
 */
static struct task* task_get(int pid)
{
    struct task* result = 0;

    if(pid == idle_task->pid)
        result = idle_task;

    if(!result) {
        if(pid == current_task->pid)
            result = current_task;
    }

    if(!result) {
        list_foreach(task, task, &ready_queue, node) {
            if(task->pid == pid) {
                result = task;
                break;
            }
        }
    }

    if(!result) {
        list_foreach(task, task, &sleeping_queue, node) {
            if(task->pid == pid) {
                result = task;
                break;
            }
        }
    }

    return result;
}

/*
 * Put current task into sleeping queue
 */
void task_block(int canrecv_port, int cansend_port, unsigned timeout)
{
    assert(!interrupts_enabled());

    current_task->wait_canrecv_port = canrecv_port;
    current_task->wait_cansend_port = cansend_port;
    if(timeout != SLEEP_INFINITE) {
        current_task->sleep_deadline = 0;
    } else {
        current_task->sleep_deadline = timer_timestamp() + timeout;
    }
    syscall(SYSCALL_BLOCK, 0, 0, 0, 0, 0);
    //trace("After sleep");
}

/*
 * Remove task from sleeping queue and put into ready queue
 */
void task_wake(int pid)
{
    assert(!interrupts_enabled());

    struct task* t = task_get(pid);
    assert(t);

    bool found = false;

    list_foreach(task, task, &sleeping_queue, node) {
        if(task == t) {
            list_remove(&sleeping_queue, task, node);
            list_append(&ready_queue, t, node);
            found = true;
            break;
        }
    }

    if(!found) {
        list_foreach(task, task, &ready_queue, node) {
            if(task == t) {
                found = true;
                break;
            }
        }
    }

    if(!found) {
        if(pid == current_task->pid)
            found = true;
    }

    if(!found) {
        if(pid == idle_task->pid) {
            found = true;
        }
    }

    if(!found) {
        panic("Failed to wake task with PID %d", pid);
    }
}

/*
 * Wake tasks waiting for port to be able to receive message
 */
void wake_tasks_waiting_for_port(int port_number)
{
    assert(!interrupts_enabled());
    
    list_foreach(task, task, &sleeping_queue, node) {
        if(task->wait_cansend_port == port_number) {
            task_wake(task->pid);
        }
    }
}

const char* current_task_name()
{
    assert(!interrupts_enabled());

    return current_task ? current_task->name : NULL;
}

void current_task_set_name(const char* name)
{
    assert(!interrupts_enabled());
    strlcpy(current_task->name, name, sizeof(current_task->name));
}

int current_task_pid()
{
    assert(!interrupts_enabled());
    return current_task ? current_task->pid : INVALID_PID;
}

bool get_task_info(struct task_info* buffer, int pid)
{
    assert(!interrupts_enabled());

    struct task* task = task_get(pid);
    if(!task)
        return false;

    buffer->pid = pid;
    strlcpy(buffer->name, task->name, sizeof(buffer->name));
    return true;
}

/*
 * Put current task into ready queue and switch to next task
 * Params
 *  ebx                     dest queue
 */
static uint32_t syscall_yield_handler(struct isr_regs* regs)
{
    list_append(&ready_queue, current_task, node);

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

    list_append(&ready_queue, new_task, node);

    result = new_task->pid;

    return result;
}

static uint32_t syscall_exit_handler(struct isr_regs* regs)
{
    /* Put into exited queue, will be collected next time scheduler runs */
    list_append(&exited_queue, current_task, node);

    task_switch_next();
    invalid_code_path();
    return 0;
}

/*
 * Sleep current task for specified number of milliseconds
 */
static uint32_t syscall_sleep_handler(struct isr_regs* regs)
{
    unsigned millis = regs->ebx;
    task_block(INVALID_PORT, INVALID_PORT, millis);
    return 0;
}

/*
 * Put current task into sleeping queue and resume next task
 */
static uint32_t syscall_block_handler(struct isr_regs* regs)
{
    list_append(&sleeping_queue, current_task, node);
    task_switch_next();
    invalid_code_path();
    return 0;
}

static uint32_t syscall_exec_handler(struct isr_regs* regs)
{
    char* filename = (char*)regs->ebx;
    assert(filename);
    assert(*filename);

    /* 
     * filename will be garbled when we unmap process memory below
     * So we copy it to temp buffer beforehand
     */
    char filename_buf[64];
    strlcpy(filename_buf, filename, sizeof(filename_buf));

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
    current_task_set_name(filename_buf);
    regs->esp = (uint32_t)(USER_STACK + PAGE_SIZE);
    regs->useresp = (uint32_t)(USER_STACK + PAGE_SIZE);
    regs->eflags = read_eflags() | EFLAGS_IF;
    regs->eip = (uint32_t)entry;

    struct task* t = task_get(current_task_pid());
    assert(t == current_task);
    assert(!strcmp(t->name, filename_buf));

    return 0;
}

/*
 * mmap
 * Params:
 *  ebx         addr
 *  ecx         size
 *  edx         flags
 * Returns:
 *  NULL        failure
 *  else        address
 */
static uint32_t syscall_mmap_handler(struct isr_regs* regs)
{
    uint32_t result = 0;

    void* addr = (void*)regs->ebx;
    size_t size = (size_t)regs->ecx;
    uint32_t flags = (uint32_t)regs->edx;

    /* Must be aligned */
    if(!IS_ALIGNED(addr, PAGE_SIZE) ||
       !IS_ALIGNED(size, PAGE_SIZE)) {
        return 0;
    } else if(flags == 0) {
        return 0;
    }

    /* Calculate flags */
    uint32_t vmm_flags = VMM_PAGE_PRESENT | VMM_PAGE_USER;
    if(flags & 0x2)
        vmm_flags |= VMM_PAGE_WRITABLE;

    /* Check validity beforehand */
    for(unsigned char* page = addr;
        page < (unsigned char*)addr + size;
        page += PAGE_SIZE) {

        /* Check if already mapped */
        uint32_t va_flags = vmm_get_flags(page);
        if(va_flags & VMM_PAGE_PRESENT) {
            return 0;
        }

        /* Check if in valid memory area */
        if((uint32_t)page < USER_START || (uint32_t)page > USER_END)
            return 0;
    }


    /* 
     * Then we can map. Note that there can still be errors,
     * but we can ignore cleaning up for now
     */
    for(unsigned char* page = addr;
        page < (unsigned char*)addr + size;
        page += PAGE_SIZE) {

        /* Then, allocate page and map */
        uint32_t frame = pmm_alloc();
        if(frame == PMM_INVALID_PAGE) {
            return 0;
        }

        vmm_map(page, frame, vmm_flags);
    }

    trace("mmap(%p, %d, %d)", addr, size, flags);

    return (uint32_t)addr;
}

static void scheduler_perform_checks()
{
    /* check1: current task and idle task should not be on any queue */
    /* check2: no two processes share pids */
    /* check3: tasks should only belong to one queue */
    uint64_t encountered_pids = 0;
    list_foreach(task, task, &ready_queue, node) {
        assert(task != current_task && task != idle_task);
        assert(!BITTEST(encountered_pids, task->pid));
        BITSET(encountered_pids, task->pid);
    }

    list_foreach(task, task, &sleeping_queue, node) {
        assert(task != current_task && task != idle_task);
        assert(!BITTEST(encountered_pids, task->pid));
        BITSET(encountered_pids, task->pid);
    }

    list_foreach(task, task, &exited_queue, node) {
        assert(task != current_task && task != idle_task);
        assert(!BITTEST(encountered_pids, task->pid));
        BITSET(encountered_pids, task->pid);
    }
}

/*
 * transform current task into an user task
 */
void jump_to_usermode(void (*user_entry)())
{
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

void scheduler_start()
{
    /* Init global data */
    list_init(&ready_queue);
    list_init(&sleeping_queue);
    list_init(&exited_queue);

    /* Install scheduler timer */
    timer_schedule(scheduler_timer, NULL, 50, true);

    /* Install syscalls */
    syscall_register(SYSCALL_YIELD, syscall_yield_handler);
    syscall_register(SYSCALL_FORK, syscall_fork_handler);
    syscall_register(SYSCALL_EXIT, syscall_exit_handler);
    syscall_register(SYSCALL_SLEEP, syscall_sleep_handler);
    syscall_register(SYSCALL_EXEC, syscall_exec_handler);
    syscall_register(SYSCALL_MMAP, syscall_mmap_handler);
    syscall_register(SYSCALL_BLOCK, syscall_block_handler);

    /* Map kernel stack */ 
    uint32_t stack_frame = pmm_alloc();
    vmm_map(KERNEL_STACK, stack_frame, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
    memset(KERNEL_STACK, 0xCC, PAGE_SIZE);

    tss_set_kernel_stack(KERNEL_STACK + PAGE_SIZE);

    /* Create first task (init) */
    struct task* task = task_create("kernel_task");
    task->context.cs = KERNEL_CODE_SEG;
    task->context.ds = KERNEL_DATA_SEG;
    task->context.ss = KERNEL_DATA_SEG;
    task->context.cr3 = vmm_get_physical(task->pagedir);
    task->context.esp = (uint32_t)(KERNEL_STACK + PAGE_SIZE);
    task->context.eflags = read_eflags() | EFLAGS_IF;
    task->context.eip = (uint32_t)kernel_task_entry;

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

