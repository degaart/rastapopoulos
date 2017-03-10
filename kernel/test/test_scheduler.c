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

typedef void (*task_entry_t)(void);

#define USER_STACK_START    0xBFFFF000
#define USERSPACE_START     0x400000

static struct process proc_table[64] = {0};
static int current_proc_index = 0;
static int proc_count = 0;

static struct process* processes = NULL;

extern void burn_cpu_cycles(unsigned count);
extern uint32_t syscall(uint32_t eax, uint32_t ebx,
                        uint32_t ecx, uint32_t edx,
                        uint32_t esi, uint32_t edi);
static void setup_usermode();

static struct process* create_process(const char* name, task_entry_t entry_point)
{
    enter_critical_section();

    struct process* proc = proc_table + proc_count;
    bzero(proc, sizeof(struct process));

    /*
     * Process state at entry
     * mode: ring0
     * eflags: whatever | EFLAGS_IF
     * esp: 4Kb of stack accessible only in kernel mode
     * esi: pid
     * eax: user-mode entry point (for tasks using setup_usermode)
     */
    proc->pid = proc_count;
    proc->name = strdup(name);
    proc->pagedir = vmm_clone_pagedir();
    proc->kernel_stack = kmalloc(4096);
    proc->user_stack = NULL;
    proc->current_ring = Ring0;
    proc->kernel_esp = proc->kernel_stack + PAGE_SIZE - 1;
    proc->registers.eflags = read_eflags() | EFLAGS_IF;
    proc->registers.eip = (uint32_t)entry_point;
    proc->registers.esp = (uint32_t)proc->kernel_esp;
    proc->registers.esi = proc->pid;

    proc_count++;

    leave_critical_section();

    return proc;
}

static void switch_process(struct process* proc)
{
    struct iret ctx = {0};

    tss_set_kernel_stack(proc->kernel_esp);

    if(proc->current_ring == Ring0) {
        ctx.cs = KERNEL_CODE_SEG;
        ctx.ds = KERNEL_DATA_SEG;
        ctx.ss = KERNEL_DATA_SEG;
    } else {
        ctx.cs = USER_CODE_SEG | RPL3;
        ctx.ds = USER_DATA_SEG | RPL3;
        ctx.ss = USER_DATA_SEG | RPL3;
    }

    uint32_t frame = vmm_get_physical(proc->pagedir);
    ctx.cr3 = frame;

    vmm_copy_kernel_mappings(proc->pagedir);

    ctx.esp = proc->registers.esp;
    ctx.eflags = proc->registers.eflags;

    ctx.eip = proc->registers.eip;
    ctx.edi = proc->registers.edi;
    ctx.esi = proc->registers.esi;
    ctx.edx = proc->registers.edx;
    ctx.ecx = proc->registers.ecx;
    ctx.ebx = proc->registers.ebx;
    ctx.eax = proc->registers.eax;
    ctx.ebp = proc->registers.ebp;

    iret(&ctx);
    
    panic("Invalid code path");
}

static void save_process_state(struct process* process, const struct isr_regs* regs)
{
    /* save current task state */
    memcpy(&process->registers, regs, sizeof(*regs));
    process->current_ring = (regs->cs & 0x3) ? Ring3 : Ring0;

    if(process->current_ring == Ring3) {
        /* esp refers to kernel stack, useresp was pushed by processor upon interrupt */
        process->registers.esp = regs->useresp; 
    } else {
        /* sizeof(eip, cs, eflags, useresp, ss), pushed by processor upon interrupt */
        process->registers.esp = regs->esp + (5 * sizeof(uint32_t));
        process->kernel_esp = (void*)regs->esp;
    }
}

#define SYSCALL_YIELD   0
#define SYSCALL_TRACE   1
#define SYSCALL_FORK    2

static void syscall_fork_handler(struct process* proc, struct isr_regs* regs)
{
    char name[64];
    snprintf(name, sizeof(name), "task%d", proc_count);

    enter_critical_section();
    struct process* new_process = create_process(name, NULL);
    new_process->user_stack = proc->user_stack;
    new_process->current_ring = proc->current_ring;
    new_process->kernel_esp = proc->kernel_esp;
    memcpy(&new_process->registers, regs, sizeof(struct isr_regs));
    
    dump_var(regs->useresp);
    dump_var(regs->esp);

    new_process->registers.esp = regs->useresp;
    new_process->registers.eax = 0;
    regs->eax = new_process->pid;
    leave_critical_section();
}

static void int80_handler(struct isr_regs* regs)
{
    struct process* current_process = proc_table + current_proc_index;

    switch(regs->eax) {
        case SYSCALL_YIELD:
            sti();
            hlt();
            break;
        case SYSCALL_TRACE:
        {
            char* buffer = kmalloc(1024);
            snprintf(buffer, 1024, "(%s) %s", current_process->name, regs->ebx);
            trace(buffer);
            kfree(buffer);
            break;
        }
        case SYSCALL_FORK:
        {
            syscall_fork_handler(current_process, regs);
            break;
        }
        default:
            panic("Invalid syscall %d", regs->eax);
            break;
    }
}

static void scheduler_timer(void* data, const struct isr_regs* regs)
{
    /* save current process state */
    struct process* current_process = proc_table + current_proc_index;
    save_process_state(current_process, regs);

    /* switch to next task */
    current_proc_index = (current_proc_index + 1) % proc_count;
    struct process* next_task = proc_table + current_proc_index;

    // trace("Switching to %s", next_task->name);
    switch_process(proc_table + current_proc_index);

    panic("Invalid code path");
}

#define CMOS_ADDRESS            0x70
#define CMOS_DATA               0x71

static unsigned cmos_read_reg(unsigned reg)
{
    outb(CMOS_ADDRESS, (1 << 7) | reg);
    unsigned result = inb(CMOS_DATA);
    return result;
}

static unsigned cmos_seconds()
{
    unsigned result = cmos_read_reg(0x00);
    return result;
}

/*
 * Burn cpu cycles
 */
unsigned USERFUNC wait(unsigned seconds)
{
    volatile unsigned result = 0;
    for(unsigned i = 2; i < seconds; i++) {
        if((seconds % i == 0)) {
            result++;
        }
    }
    return result;
}

#define WAIT_FACTOR 5000000

/*
 * Set up user-mode stack and jump to user-mode procedure
 */
static void setup_usermode()
{
    int pid = read_esi();
    struct process* proc = proc_table + pid;

    trace("%s started", proc->name);

    proc->kernel_esp = (void*)read_esp();
    proc->user_stack = (void*)USER_STACK_START;
    vmm_map(proc->user_stack, pmm_alloc(), VMM_PAGE_PRESENT|VMM_PAGE_WRITABLE|VMM_PAGE_USER);
    memset(proc->user_stack, 0xCC, PAGE_SIZE);

    proc->registers.esp = (uint32_t)proc->user_stack + PAGE_SIZE - 1;
    proc->registers.eip = proc->registers.eax;
    proc->current_ring = Ring3;

    current_proc_index = 1;
    switch_process(proc);

    panic("Invalid code path");
}

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

void USERFUNC usermode_entry2()
{
    volatile unsigned counter = 0;
    char msg[64];
    while(1) {
        wait(WAIT_FACTOR / 3);

        static const char USERDATA fmt[] = "counter: %d";
        snprintf(msg, sizeof(msg), fmt, counter);

        syscall(SYSCALL_TRACE, (uint32_t)msg, 0, 0, 0, 0);
        counter++;
    }
}

void USERFUNC usermode_entry1()
{
    volatile unsigned counter = 0;
    char msg[64];
    while(1) {
        wait(WAIT_FACTOR / 3);

        static const char USERDATA fmt[] = "counter: %d";
        snprintf(msg, sizeof(msg), fmt, counter);

        syscall(SYSCALL_TRACE, (uint32_t)msg, 0, 0, 0, 0);
        counter++;
    }
}

void USERFUNC usermode_entry0()
{
    volatile unsigned counter = 0;
    char msg[64];
    while(1) {
        wait(WAIT_FACTOR / 2);

        static const char USERDATA fmt[] = "counter: %d";
        snprintf(msg, sizeof(msg), fmt, counter);

        syscall(SYSCALL_TRACE, (uint32_t)msg, 0, 0, 0, 0);
        counter++;

        if(counter == 2) {
            unsigned pid = syscall(SYSCALL_FORK, 0, 0, 0, 0, 0);
            if(pid) {
                static const char USERDATA msg1[] = "In parent";
                syscall(SYSCALL_TRACE, (uint32_t)msg1, 0, 0, 0, 0);
                while(1);
            } else {
                static const char USERDATA msg2[] = "In child";
                syscall(SYSCALL_TRACE, (uint32_t)msg2, 0, 0, 0, 0);
                while(1);
            }
        }
    }
}

static void kernel_task_entry()
{
    int pid = read_esi();
    struct process* proc = proc_table + pid;

    struct task_data* data = (struct task_data*)0x400000;

    trace("%s started", proc->name);
    int counter = 0;
    while(counter < 10) {
        wait(WAIT_FACTOR);
        trace("%s: %d", proc->name, counter);
        syscall(SYSCALL_YIELD, 0, 0, 0, 0, 0);
        counter++;

        if(counter == 2) {
            enter_critical_section();
            struct process* proc = create_process("task1", setup_usermode);
            proc->registers.eax = (uint32_t)usermode_entry0;
            leave_critical_section();
        }/* else if(counter == 4) {
            enter_critical_section();
            struct process* proc = create_process("task2", setup_usermode);
            proc->registers.eax = (uint32_t)usermode_entry1;
            leave_critical_section();
        } else if(counter == 6) {
            enter_critical_section();
            struct process* proc = create_process("task3", setup_usermode);
            proc->registers.eax = (uint32_t)usermode_entry2;
            leave_critical_section();
        }*/
    }

    trace("%s done", proc->name);
    reboot();
}

static void test_fork()
{
    /*
     * Memory layout for each task
     *
     * --------------------------------   <--- 0x0000
     * | shared kernel space (low)    |
     * --------------------------------   <--- 4MB
     * | private user space           |
     * --------------------------------   <--- 3GB - 4KB (0xBFFFF000)
     * | user stack                   |
     * --------------------------------   <--- 3GB
     * | shared kernel space (high)   |
     * --------------------------------   <--- 4GB - 1
     */

    /*
     * Tests:
     * Create kernel_task
     * kernel_task forks to create two new processes
     * ring0 only for now
     * preempt task only on syscalls
     */

    /* install int 0x80 handler */
    idt_install(0x80, int80_handler, true);

    /* Add scheduler timer */
    timer_schedule(scheduler_timer, NULL, 50, true);

    /* Create kernel_task */
    struct process* proc = create_process("kernel_task", kernel_task_entry);

    /* Switch to task0 */
    switch_process(proc);
}

static struct pagedir* pagedirs[2];
static struct iret contexts[2];
static int current_context = 0;
extern void save_context(struct iret* context);

static void timer1(void* data, const struct isr_regs* regs)
{
    /* Save current process context */
    struct iret* ctx = contexts + current_context;
    ctx->esp = regs->esp + (5 * sizeof(uint32_t));               /* TODO: Check this value */
    ctx->eflags = regs->eflags;
    ctx->eip = regs->eip;
    ctx->edi = regs->edi;
    ctx->esi = regs->esi;
    ctx->edx = regs->edx;
    ctx->ecx = regs->ecx;
    ctx->ebx = regs->ebx;
    ctx->eax = regs->eax;
    ctx->ebp = regs->ebp;

    current_context = (current_context + 1) % 2;

    ctx = contexts + current_context;
    assert(ctx->ss != 0);
    //ctx->eflags &= ~EFLAGS_IF;

    // TODO: vmm_copy_kernel_mappings
    vmm_copy_kernel_mappings(pagedirs[current_context]);
    iret(ctx);
    
    panic("Invalid code path");
}

static unsigned entry1()
{
    volatile unsigned in_parent = 0xDEADBEEF;
    uint32_t esp = read_esp();
    dump_var(read_cr3());
    dump_var(esp);

    struct iret* new_ctx = contexts + 1;
    memcpy(new_ctx, contexts, sizeof(struct iret));

    new_ctx->eflags = read_eflags();
    asm volatile("movl %%ebp, %0" : "=a"(new_ctx->ebp));
    asm volatile("movl %%ebx, %0" : "=a"(new_ctx->ebx));
    asm volatile("movl %%esi, %0" : "=a"(new_ctx->esi));
    asm volatile("movl %%edi, %0" : "=a"(new_ctx->edi));

    /* Clone current pagedir */
    struct pagedir* new_pagedir = vmm_clone_pagedir();
    pagedirs[1] = new_pagedir;
    new_ctx->esp = read_esp();
    in_parent = 0xB16B00B5;

    new_ctx->cr3 = (uint32_t)vmm_get_physical(new_pagedir);
    vmm_copy_kernel_mappings(new_pagedir);

    new_ctx->eip = (uint32_t)&&after_clone;

    //iret(new_ctx);

    /* Stack state is the state upon calling vmm_clone_pagedir() */
after_clone:
    dump_var(in_parent);
    dump_var(read_cr3());
    dump_var(read_esp());
    return in_parent == 0xDEADBEEF;

    //reboot();
}

static void entry0()
{
    unsigned int canary = 0xDEADBEEF;

#if 1
    timer_schedule(timer1, NULL, 50, true);

    unsigned counter = 0;
    while(1) {
        wait(WAIT_FACTOR);
        dump_var(read_cr3());
        counter++;
        if(counter == 2) {
            unsigned ret = entry1();
            dump_var(ret);
            if(ret)
                trace("Here");
            //else
                sti();
        }
    }
#else
    entry1();
    dump_var(canary);
#endif
    reboot();
}

static void test_fork2()
{
    /* Map kernel stack */ 
    uint32_t stack_frame = pmm_alloc();
    vmm_map((void*)0xBFFFF000, stack_frame, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
    memset((void*)0xBFFFF000, 0xCC, PAGE_SIZE);

    /* Clone current pagedir */
    struct pagedir* pagedir = vmm_clone_pagedir();
    pagedirs[0] = pagedir;

    struct iret* context = contexts;
    bzero(context, sizeof(struct iret));

    context->cs = KERNEL_CODE_SEG;
    context->ds = context->ss = KERNEL_DATA_SEG;
    context->cr3 = (uint32_t)vmm_get_physical(pagedir);
    context->esp = 0xC0000000;
    context->eflags = read_eflags();
    context->eip = (uint32_t)entry0;

    iret(context);
    panic("Invalid code path");
}

void test_scheduler()
{
    trace("Testing scheduler");

    //test_fork();
    test_fork2();
}


