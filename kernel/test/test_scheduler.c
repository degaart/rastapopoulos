#include "../process.h"
#include "../debug.h"
#include "../util.h"
#include "../pmm.h"
#include "../vmm.h"
#include "../kernel.h"
#include "../string.h"
#include "../gdt.h"
#include "../context.h"
#include "../kmalloc.h"
#include "../registers.h"
#include "../timer.h"
#include "../io.h"
#include "../locks.h"

/*
 * Address-space layout for each task
 *
 *  ---------------------------------------------   0x00000000
 *   shared low kernel space
 *  ---------------------------------------------   0x00400000
 *   process-specific user space
 *  ---------------------------------------------
 *   free address space
 *  ---------------------------------------------   0xBFFFC000
 *   process-specific user stack
 *  ---------------------------------------------   0xBFFFD000
 *   guard page
 *  ---------------------------------------------   0xBFFFE000
 *   process-specific kernel stack
 *  ---------------------------------------------   0xBFFFF000
 *   guard page
 *  ---------------------------------------------   0xC0000000
 *   shared high kernel space
 *  ---------------------------------------------   0xFFFFFFFF
 */

struct task {
    int pid;
    char name[32];
    struct pagedir* pagedir;
    struct context context;
    struct task* next;
};

#define USER_STACK          ((unsigned char*)0xBFFFC000)
#define KERNEL_STACK        ((unsigned char*)0xBFFFE000)

static struct task* tasks = NULL;
static struct task* kernel_task = NULL;
static int next_pid_value = 0;
static struct task* current_task = NULL;
static struct task* exited_tasks = NULL;

static volatile int mixed_done = 0;
static volatile int USERDATA user_done = 0;

extern uint32_t syscall(uint32_t eax, uint32_t ebx,
                        uint32_t ecx, uint32_t edx,
                        uint32_t esi, uint32_t edi);

unsigned USERFUNC is_prime(unsigned num)
{
    if(num == 1)
        return 1;

    for(unsigned i = 2; i < num; i++) {
        if(!(num % i))
            return 0;
    }

    return 1;
}

static struct task* task_create(const char* name)
{
    struct task* result = kmalloc(sizeof(struct task));
    bzero(result, sizeof(struct task));

    enter_critical_section();
    strlcpy(result->name, name, sizeof(result->name));
    result->pid = next_pid_value++;
    result->pagedir = vmm_clone_pagedir();
    result->next = tasks;
    tasks = result;
    leave_critical_section();

    return result;
}

static unsigned kfork()
{
    volatile unsigned result = 0;
    uint32_t esp = read_esp();
    dump_var(read_cr3());
    dump_var(esp);

    enter_critical_section();

    struct task* new_task = task_create(current_task->name);
    new_task->context.esp = read_esp();
    new_task->context.eflags = read_eflags();
    new_task->context.ebp = read_ebp();
    new_task->context.ebx = read_ebx();         /* maybe not needed because of the barrier() */ 
    new_task->context.esi = read_esi();         /* maybe not needed because of the barrier() */
    new_task->context.edi = read_edi();         /* maybe not needed because of the barrier() */
    new_task->context.cs = KERNEL_CODE_SEG;
    new_task->context.ds = new_task->context.ss = KERNEL_DATA_SEG;
    new_task->context.cr3 = vmm_get_physical(new_task->pagedir);
    new_task->context.eip = (uint32_t)&&after_clone;
    result = new_task->pid;

after_clone:
    barrier();  /* re-read all variables from stack */
    leave_critical_section();
    return result;
}

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

#define SYSCALL_TRACE       0
#define SYSCALL_EXIT        1
#define SYSCALL_FORK        2

static int syscall_trace_handler(struct isr_regs* regs)
{
    char* buffer = kmalloc(1024);
    snprintf(buffer, 1024, "(%d) %s", current_task->pid, regs->ebx);
    trace(buffer);
    kfree(buffer);
    return 0;
}

static int syscall_exit_handler(struct isr_regs* regs)
{
    /*
     * TODO: Remove this syscall and instead send message
     * to kernel_task
     */
    struct task* task = current_task;

    /* 
     * Cannot destroy task inside itself 'cause then the pagedir would be invalidated
     * Instead, place it into a task recycle bin and let kernel_task handle its destruction
     */
    assert(task != kernel_task);

    /* Get next task to run */
    current_task = task->next;
    if(!current_task)
        current_task = tasks;
    assert(current_task != task);

    /* Remove task from our list */
    struct task* prevt = NULL;
    struct task* t = NULL;
    for(t = tasks; t && t != task; prevt = t, t = t->next);
    assert(t);

    if(prevt)
        prevt->next = task->next;
    else
        tasks = task->next;

    /* Add to exited tasks */
    task->next = exited_tasks;
    exited_tasks = task;

    /* Switch to next available task */
    vmm_copy_kernel_mappings(current_task->pagedir);
    tss_set_kernel_stack(KERNEL_STACK + PAGE_SIZE);
    switch_context(&current_task->context);

    panic("Invalid code path");
    return 0;
}

static int syscall_fork_handler(struct isr_regs* regs)
{
    unsigned pid = kfork();
    return pid;
}

static void syscall_handler(struct isr_regs* regs)
{
#define X(syscall, fn)          \
    case syscall:               \
        regs->eax = fn(regs);   \
        break;

    unsigned syscall_num = regs->eax;
    switch(syscall_num) {
        X(SYSCALL_TRACE, syscall_trace_handler);
        X(SYSCALL_EXIT, syscall_exit_handler);
        X(SYSCALL_FORK, syscall_fork_handler);
        default:
            panic("Invalid syscall: %d", syscall_num);
            break;
    }
#undef X
}

static void scheduler_timer(void* data, const struct isr_regs* regs)
{
    assert(current_task);

    save_task_state(current_task, regs);

    if(current_task->next)
        current_task = current_task->next;
    else
        current_task = tasks;

    assert(current_task);

    vmm_copy_kernel_mappings(current_task->pagedir);

    tss_set_kernel_stack(KERNEL_STACK + PAGE_SIZE);
    switch_context(&current_task->context);
    
    panic("Invalid code path");
}

static void USERFUNC user_entry()
{
    unsigned start_val = 5001000;

    /* Fork into two other tasks */
    unsigned pid = syscall(SYSCALL_FORK, 0, 0, 0, 0, 0);
    if(!pid) {
        start_val = 5002000;
    } else {
        pid = syscall(SYSCALL_FORK, 0, 0, 0, 0, 0);
        if(!pid)
            start_val = 5003000;
    }

    unsigned end_val = start_val + 500;
    for(unsigned i = start_val; i < end_val; i++) {
        if(is_prime(i)) {
            static char USERDATA msg[64];
            static const char USERRODATA fmt[] = "%d";
            snprintf(msg, sizeof(msg), fmt, i);
            syscall(SYSCALL_TRACE, (uint32_t)msg, 0, 0, 0, 0);
        }
    }

    static char done_msg[] = "Done";
    syscall(SYSCALL_TRACE, (uint32_t)done_msg, 0, 0, 0, 0);

    syscall(SYSCALL_EXIT, 0, 0, 0, 0, 0);
    panic("Invalid code path");             /* Should give a page fault */
    while(1);
}

static void create_user_task()
{
    struct task* task = current_task;

    strlcpy(task->name, "usertask", sizeof(task->name));
    task->context.cs = USER_CODE_SEG | RPL3;
    task->context.ds = task->context.ss = USER_DATA_SEG | RPL3;
    vmm_map(USER_STACK, pmm_alloc(), VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE | VMM_PAGE_USER);
    memset(USER_STACK, 0xCC, PAGE_SIZE);

    task->context.esp = (uint32_t)(USER_STACK + PAGE_SIZE);

    task->context.eflags |= EFLAGS_IF;
    task->context.eip = (uint32_t)user_entry;

    switch_context(&task->context);
    panic("Invalid code path");
}

static void kernel_task_entry()
{
    /* initialization: create first user task, which will fork into 3 instances */
    unsigned pid = kfork();
    if(!pid) {
        create_user_task();
        panic("Invalid code path");
    }

    /* Sit idle until we get a task to reap */
    while(1) {
        hlt();

        enter_critical_section();
        if(exited_tasks) {
            struct task* task = exited_tasks;
            while(task) {
                struct task* next = task->next;
                vmm_destroy_pagedir(task->pagedir);
                kfree(task);

                task = next;
            }
            exited_tasks = NULL;
        }
        if(!tasks->next)
            break;
        leave_critical_section();
    }
    reboot();
    panic("Invalid code path");
}

static void test_fork()
{
    /* Install scheduler timer */
    timer_schedule(scheduler_timer, NULL, 50, true);

    /* Install syscall handler */
    idt_install(0x80, syscall_handler, true);

    /* Map kernel stack */ 
    uint32_t stack_frame = pmm_alloc();
    vmm_map(KERNEL_STACK, stack_frame, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
    memset(KERNEL_STACK, 0xCC, PAGE_SIZE);

    tss_set_kernel_stack(KERNEL_STACK + PAGE_SIZE);

    /* Create kernel_task */
    struct task* task = task_create("kernel_task");
    task->context.cs = KERNEL_CODE_SEG;
    task->context.ds = KERNEL_DATA_SEG;
    task->context.ss = KERNEL_DATA_SEG;
    task->context.cr3 = vmm_get_physical(task->pagedir);
    task->context.esp = (uint32_t)(KERNEL_STACK + PAGE_SIZE);
    task->context.eflags = read_eflags() | EFLAGS_IF;
    task->context.eip = (uint32_t)kernel_task_entry;

    current_task = task;
    kernel_task = task;

    switch_context(&task->context);
    panic("Invalid code path");
}

void test_scheduler()
{
    trace("Testing scheduler");

    test_fork();
}



