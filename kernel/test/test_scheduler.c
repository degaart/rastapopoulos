#include "../process.h"
#include "../debug.h"
#include "../util.h"
#include "../pmm.h"
#include "../vmm.h"
#include "../kernel.h"
#include "../string.h"
#include "../gdt.h"
#include "../iret.h"
#include "../kmalloc.h"
#include "../registers.h"
#include "../timer.h"
#include "../io.h"
#include "../locks.h"

/*********************************************************************************************************
 * So the plan is to create 3 tasks
 *  task0 stays in kernel mode all the time
 *  task1 stays in user-mode all the time
 *  task2 swings back and forth from kernel to user-mode
 *********************************************************************************************************/

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
    struct pagedir* pagedir;
    struct iret context;
    struct task* next;
};

#define USER_STACK          ((unsigned char*)0xBFFFC000)
#define KERNEL_STACK        ((unsigned char*)0xBFFFE000)

static struct task* tasks = NULL;
static int next_pid_value = 0;
static struct task* current_task = NULL;

static volatile int mixed_done = 0;
static volatile int USERDATA user_done = 0;

extern uint32_t syscall(uint32_t eax, uint32_t ebx,
                        uint32_t ecx, uint32_t edx,
                        uint32_t esi, uint32_t edi);

static void USERFUNC user_outb(uint16_t port, uint8_t val)
{
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

static uint8_t USERFUNC user_inb(uint16_t port)
{
    uint8_t ret;
    asm volatile ( "inb %1, %0"
                   : "=a"(ret)
                   : "Nd"(port) );
    return ret;
}

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

static struct task* task_create()
{
    struct task* result = kmalloc(sizeof(struct task));
    bzero(result, sizeof(struct task));

    enter_critical_section();
    result->pid = next_pid_value++;
    result->pagedir = vmm_clone_pagedir();
    result->next = tasks;
    tasks = result;
    leave_critical_section();

    return result;
}

static unsigned fork()
{
    volatile unsigned result = 0;
    uint32_t esp = read_esp();
    dump_var(read_cr3());
    dump_var(esp);

    enter_critical_section();

    struct task* new_task = task_create();
    new_task->context.eflags = read_eflags();
    asm volatile("movl %%ebp, %0" : "=a"(new_task->context.ebp));
    asm volatile("movl %%ebx, %0" : "=a"(new_task->context.ebx));
    asm volatile("movl %%esi, %0" : "=a"(new_task->context.esi));
    asm volatile("movl %%edi, %0" : "=a"(new_task->context.edi));
    new_task->context.cs = KERNEL_CODE_SEG;
    new_task->context.ds = new_task->context.ss = KERNEL_DATA_SEG;
    new_task->context.esp = read_esp();
    new_task->context.cr3 = vmm_get_physical(new_task->pagedir);
    vmm_copy_kernel_mappings(new_task->pagedir);
    new_task->context.eip = (uint32_t)&&after_clone;
    result = new_task->pid;

after_clone:
    asm volatile ("" : : : "memory");       /* re-read all variables */
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

static void syscall_fork_handler(struct isr_regs* regs)
{
    unsigned pid = fork();
    regs->eax = pid;
}

static void syscall_handler(struct isr_regs* regs)
{
    unsigned syscall_num = regs->eax;
    switch(syscall_num) {
        case SYSCALL_TRACE:
        {
            char* buffer = kmalloc(1024);
            snprintf(buffer, 1024, "(%d) %s", current_task->pid, regs->ebx);
            trace(buffer);
            kfree(buffer);
            break;
        }
        case SYSCALL_EXIT:
        {
            mixed_done = 1;
            break;
        }
        case SYSCALL_FORK:
        {
            syscall_fork_handler(regs);
            break;
        }
        default:
            panic("Invalid syscall: %d", syscall_num);
            break;
    }
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
    iret(&current_task->context);
    
    panic("Invalid code path");
}

static void ring0()
{
    unsigned start_val = 5001000;
    unsigned end_val = start_val + 1000;
    for(unsigned i = start_val; i < end_val; i++) {
        if(is_prime(i)) {
            trace("%d", i);
        }
    }

    while(!mixed_done && !user_done)
        hlt();

    reboot();
}

static void USERFUNC ring3()
{
    unsigned start_val = 5003000;
    unsigned end_val = start_val + 1000;
    for(unsigned i = start_val; i < end_val; i++) {
        if(is_prime(i)) {
            /* Do nothing */
            static char USERDATA msg[64];
            static const char USERRODATA fmt[] = "[%s:%d][%s] %d\n";
            static const char USERRODATA file[] = "test_scheduler.c";
            static const char USERRODATA func[] = "ring3";

            snprintf(msg, sizeof(msg), fmt, file, __LINE__, func, i);
            for(const char* p = msg; *p; p++)
                user_outb(0xE9, *p);
        }
    }
    user_done = 1;
    while(1);
}

static void ring3_thunk()
{
    struct task* task = current_task;
    task->context.cs = USER_CODE_SEG | RPL3;
    task->context.ds = task->context.ss = USER_DATA_SEG | RPL3;
    vmm_map(USER_STACK, pmm_alloc(), VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE | VMM_PAGE_USER);
    memset(USER_STACK, 0xCC, PAGE_SIZE);
    task->context.esp = (uint32_t)(USER_STACK + PAGE_SIZE);

    task->context.eflags |= EFLAGS_IF;
    task->context.eip = (uint32_t)ring3;

    iret(&task->context);
}

static void USERFUNC mixed()
{
    unsigned pid = syscall(SYSCALL_FORK, 0, 0, 0, 0, 0);
    if(!pid) {
        ring3();
        return;
    }

    unsigned start_val = 5002000;
    unsigned end_val = start_val + 1000;
    for(unsigned i = start_val; i < end_val; i++) {
        if(is_prime(i)) {
            static char USERDATA msg[64];
            static const char USERRODATA fmt[] = "%d";
            snprintf(msg, sizeof(msg), fmt, i);
            syscall(SYSCALL_TRACE, (uint32_t)msg, 0, 0, 0, 0);
        }
    }
    syscall(SYSCALL_EXIT, 0, 0, 0, 0, 0);
    while(1);
}

static void mixed_thunk()
{
    struct task* task = current_task;
    task->context.cs = USER_CODE_SEG | RPL3;
    task->context.ds = task->context.ss = USER_DATA_SEG | RPL3;
    vmm_map(USER_STACK, pmm_alloc(), VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE | VMM_PAGE_USER);
    memset(USER_STACK, 0xCC, PAGE_SIZE);
    task->context.esp = (uint32_t)(USER_STACK + PAGE_SIZE);

    task->context.eflags |= EFLAGS_IF;
    task->context.eip = (uint32_t)mixed;

    iret(&task->context);
}

static void entry0()
{
    unsigned ret = fork();
    if(ret) {
        ring0();
    } else {
        mixed_thunk();
    }
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

    /* Create first task */
    struct task* task = task_create();
    task->context.cs = KERNEL_CODE_SEG;
    task->context.ds = KERNEL_DATA_SEG;
    task->context.ss = KERNEL_DATA_SEG;
    task->context.cr3 = vmm_get_physical(task->pagedir);
    task->context.esp = (uint32_t)(KERNEL_STACK + PAGE_SIZE);
    task->context.eflags = read_eflags() | EFLAGS_IF;
    task->context.eip = (uint32_t)entry0;

    current_task = task;

    iret(&task->context);
    panic("Invalid code path");
}

void test_scheduler()
{
    trace("Testing scheduler");

    test_fork();
}


