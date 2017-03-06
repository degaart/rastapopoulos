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

static struct process proc_table[2] = {0};
static int current_proc_index = 0;

extern void burn_cpu_cycles(unsigned count);

static void scheduler_timer(void* data, const struct isr_regs* regs)
{

}

static void switch_process(struct process* proc)
{
    struct iret ctx = {0};
    ctx.cs = KERNEL_CODE_SEG;
    ctx.ds = KERNEL_DATA_SEG;
    ctx.ss = KERNEL_DATA_SEG;
    ctx.cr3 = (uint32_t)proc->pagedir;
    ctx.esp = (uint32_t)proc->kernel_esp;
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
    
    assert(!"Invalid code path");
}

static void int80_handler(struct isr_regs* regs)
{
    /* save current task state */
    struct process* current_task = proc_table + current_proc_index;
    memcpy(&current_task->registers, regs, sizeof(*regs));
    current_task->kernel_esp = (void*)regs->esp;

    /* switch to next task */
    current_proc_index = (current_proc_index + 1) % 2;
    switch_process(proc_table + current_proc_index);

    trace("Invalid code path");
    abort();
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
 * busy-wait for specified number of seconds
 */
static void wait(unsigned seconds)
{
    unsigned start_seconds = cmos_seconds();
    for(unsigned i = 0; i < seconds; i++) {
        while(cmos_seconds() == start_seconds);
    }
}

struct task_data {
    unsigned counter;
};

static void kernel_task_entry()
{
    int pid = read_esi();
    struct process* proc = proc_table + pid;

    struct task_data* data = (struct task_data*)0x400000;
    if(vmm_get_physical(data))
        vmm_unmap(data);

    uint32_t frame = pmm_alloc();
    vmm_map(data, frame, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE | VMM_PAGE_USER);

    data->counter = 0;

    trace("%s started", proc->name);
    while(data->counter < 10) {
        trace("%s: %d", proc->name, data->counter);
        wait(1);
        data->counter++;

        if(data->counter % 2) {
            asm volatile("int $0x80");
        }
    }

    vmm_unmap(data);
    reboot();
}

static void usermode_entry() __attribute__((align(4096)))
{
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
     * --------------------------------   <--- 3GB - 64KB - ???
     * | user stack                   |
     * --------------------------------   <--- 3GB
     * | shared kernel space (high)   |
     * --------------------------------   <--- 4GB - 1
     */

    /* install int 0x80 handler */
    idt_install(0x80, int80_handler, true);

    /* Create first task0 */
    struct process* proc0 = proc_table;
    proc0->pid = 0;
    proc0->name = "task0";
    proc0->pagedir = vmm_clone_pagedir();
    proc0->kernel_stack = kmalloc(65536);
    proc0->user_stack = (void*)0xBFFE0000;                    /* 64KB of user stack */
    proc0->current_ring = Ring0;
    proc0->kernel_esp = proc0->kernel_stack + 65536;
    proc0->registers.eflags = read_eflags() /* | EFLAGS_IF */;
    proc0->registers.eip = (uint32_t)kernel_task_entry;
    proc0->registers.esp = (uint32_t)proc0->kernel_esp;
    proc0->registers.esi = 0;

    /* Create task1 */
    struct process* proc1 = proc_table + 1;
    proc1->pid = 1;
    proc1->name = "task1";
    proc1->pagedir = vmm_clone_pagedir();
    proc1->kernel_stack = kmalloc(65536);
    proc1->user_stack = (void*)0xBFFE0000;                    /* 64KB of user stack */
    proc1->current_ring = Ring0;
    proc1->kernel_esp = proc1->kernel_stack + 65536;
    proc1->registers.eflags = read_eflags() /* | EFLAGS_IF */;
    proc1->registers.eip = (uint32_t)kernel_task_entry;
    proc1->registers.esp = (uint32_t)proc1->kernel_esp;
    proc1->registers.esi = 1;


    /* Switch to task0 */
    switch_process(proc0);
}


