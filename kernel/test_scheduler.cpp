#include "process.h"
#include "regs.h"
#include "string.h"
#include "gdt.h"
#include "pagedir.h"
#include "util.h"
#include "context.h"

static void iret_proc() {
    TRACE("Into iret_proc");
    while(true) {
        yield();
        TRACE("Sill here");
    }
}

static void test_switch_context() {
    TRACE("Testing switch_context");

    static uint8_t* context_stack = (uint8_t*) (VMM::USERSPACE_END - VMM::PAGE_SIZE + 1);
    assert( is_aligned(context_stack) );

    Pagedir* pagedir = VMM::create_pagedir();
    pagedir->alloc(context_stack, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE);
    pagedir->copy_kernel_mappings(VMM::current_pagedir());

    uint32_t current_cr3;
    read_cr3(current_cr3);
    TRACE("cr3: 0x%X -> 0x%X", current_cr3, pagedir->physical());
    write_cr3(pagedir->physical()); // Notice this will work because the stack is in kernel-space at this point

    context_t context;
    bzero(&context, sizeof(context));
    context.cs = KERNEL_CODE_SEG;
    context.ds = KERNEL_DATA_SEG;
    context.ss = KERNEL_DATA_SEG;
    context.pagedir = pagedir;

    context.regs.esp = (uint32_t)(context_stack + VMM::PAGE_SIZE - 1);
    read_eflags(context.regs.eflags); 
    context.regs.eflags |= EFLAGS_IF;
    context.regs.eip = (uint32_t)iret_proc;
    
    context.regs.edi = 0x01010101;
    context.regs.esi = 0x02020202;
    context.regs.edx = 0x03030303;
    context.regs.ecx = 0x04040404;
    context.regs.ebx = 0x05050505;
    context.regs.eax = 0x06060606;
    context.regs.ebp = 0x07070707;
    switch_context(&context);
}

void test_scheduler() {
    test_switch_context();    
}



