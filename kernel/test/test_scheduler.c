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

#define USERFUNC __attribute__((section(".user")))
#define USERDATA __attribute__((section(".userdata")))

static struct process proc_table[10] = {0};
static int current_proc_index = 0;
static int proc_count = 0;

extern void burn_cpu_cycles(unsigned count);
extern uint32_t syscall(uint32_t eax, uint32_t ebx,
                        uint32_t ecx, uint32_t edx,
                        uint32_t esi, uint32_t edi);

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

    static unsigned counter = 0;
    counter++;

    uint32_t frame = vmm_get_physical(proc->pagedir);

    ctx.cr3 = vmm_get_physical(proc->pagedir);
    vmm_switch_pagedir(proc->pagedir);

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
            const char* format = (const char*)regs->ebx;
            void* arg0 = (void*)regs->ecx;
            void* arg1 = (void*)regs->edx;
            void* arg2 = (void*)regs->esi;
            void* arg3 = (void*)regs->edi;

            char* buffer = kmalloc(1024);
            snprintf(buffer, 1024, "(%s) ", current_process->name);
            sncatf(buffer, 1024, format, arg0, arg1, arg2, arg3);
            trace(buffer);
            kfree(buffer);
            break;
        }
        default:
            panic("Invalid syscall");
            break;
    }

#if 0
    /* Switch to next task */
    current_proc_index = (current_proc_index + 1) % proc_count;
    struct process* next_task = proc_table + current_proc_index;
    switch_process(proc_table + current_proc_index);

    panic("Invalid code path");
#endif
}

static void scheduler_timer(void* data, const struct isr_regs* regs)
{
    /* save current process state */
    struct process* current_process = proc_table + current_proc_index;
    save_process_state(current_process, regs);

    /* switch to next task */
    current_proc_index = (current_proc_index + 1) % proc_count;
    struct process* next_task = proc_table + current_proc_index;
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

static void task0_entry()
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
    }

    trace("%s done", proc->name);
    reboot();
}

void USERFUNC usermode_entry();

static void task1_entry()
{
    int pid = read_esi();
    struct process* proc = proc_table + pid;

    trace("%s started", proc->name);

    proc->kernel_esp = (void*)read_esp();
    proc->user_stack = kmalloc_a(PAGE_SIZE, PAGE_SIZE);
    vmm_remap(proc->user_stack, VMM_PAGE_PRESENT|VMM_PAGE_WRITABLE|VMM_PAGE_USER);
    proc->registers.esp = (uint32_t)proc->user_stack + PAGE_SIZE - 1;
    proc->registers.eip = (uint32_t)usermode_entry;
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

char USERDATA user_message[] = "deadbeef";
char USERDATA user_format[] = "%d";

void USERFUNC usermode_entry()
{
    unsigned counter = 0;
    while(1) {
        wait(WAIT_FACTOR / 2);
        syscall(SYSCALL_TRACE, (uint32_t)user_format, counter, 0, 0, 0);
        counter++;
    }
}

void test_scheduler()
{
    trace("Testing scheduler");

    /*
     * Memory layout for each task
     *
     * --------------------------------   <--- 0x0000
     * | shared kernel space (low)    |
     * --------------------------------   <--- 4MB
     * | private user space           |
     * --------------------------------   <--- 3GB - 64KB - ??? 0xBFFE0000
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

    unsigned eflags = read_eflags() | EFLAGS_IF;

    /* Create first task0 */
    struct process* proc0 = proc_table;
    proc0->pid = 0;
    proc0->name = "task0";
    proc0->pagedir = vmm_clone_pagedir();
    proc0->kernel_stack = kmalloc_a(PAGE_SIZE, PAGE_SIZE);
    proc0->user_stack = NULL;
    proc0->current_ring = Ring0;
    proc0->kernel_esp = proc0->kernel_stack + PAGE_SIZE - 1;
    proc0->registers.eflags = eflags;
    proc0->registers.eip = (uint32_t)task0_entry;
    proc0->registers.esp = (uint32_t)proc0->kernel_esp;
    proc0->registers.esi = proc0->pid;

    /* Create second task1 */
    struct process* proc1 = proc_table + 1;
    proc1->pid = 1;
    proc1->name = "task1";
    proc1->pagedir = vmm_clone_pagedir();
    proc1->kernel_stack = kmalloc_a(PAGE_SIZE, PAGE_SIZE);
    proc1->user_stack = NULL;
    proc1->current_ring = Ring0;
    proc1->kernel_esp = proc1->kernel_stack + PAGE_SIZE - 1;
    proc1->registers.eflags = eflags;
    proc1->registers.eip = (uint32_t)task1_entry;
    proc1->registers.esp = (uint32_t)proc1->kernel_esp;
    proc1->registers.esi = proc1->pid;

    proc_count = 2;

    /* Switch to task0 */
    switch_process(proc0);
}


