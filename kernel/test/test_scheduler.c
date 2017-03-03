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

static void kernel_task_entry()
{
    unsigned esi = read_esi();
    trace("Hello and welcome to kernel_task 0x%X", esi);

    unsigned counter = 0;
    while(counter < 3) {
        trace("counter: %d", counter);
        wait(1);
        counter++;
    }
    reboot();
}

void test_scheduler()
{
    trace("Testing scheduler");

    /*
     * Create kernel_task
     * Memory layout
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
    struct process proc = {0};
    proc.pid = 0;
    proc.name = "kernel_task";
    proc.pagedir = (void*)read_cr3(); /* vmm_clone_pagedir(vmm_current_pagedir()); */
#if 0
    proc.pagedir = kmalloc_a(PAGE_SIZE, PAGE_SIZE);
    memset(proc.pagedir, 0, PAGE_SIZE);
#endif

    proc.kernel_stack = kmalloc(65536);                     /* 3GB - 64KB */
    proc.user_stack = (void*)0xBFFE0000;                    /* 64KB of user stack */
    proc.current_ring = Ring0;
    proc.kernel_esp = proc.kernel_stack + 65536;
    proc.registers.eflags = read_eflags() /* | EFLAGS_IF */;
    proc.registers.eip = (uint32_t)kernel_task_entry;
    proc.registers.esp = (uint32_t)proc.kernel_esp;
    proc.registers.esi = 0xDEADBEEF;

    switch_process(&proc);
}


