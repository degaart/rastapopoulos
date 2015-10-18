#include <stdint.h>
#include "util.h"
#include "debug.h"
#include "cxxrt.h"
#include "pmm.h"
#include "vmm.h"
#include "gdt.h"
#include "idt.h"
#include "kmalloc.h"
#include "string.h"
#include "../bootldr/kernel_params.h"
#include "heap.h"
#include "kheap.h"
#include "pic.h"
#include "pit.h"
#include "regs.h"
#include "io.h"
#include "timer.h"

extern "C"
void switch_to_usermode(
    uint32_t esp, uint32_t eflags, uint32_t eip,
    uint32_t edi, uint32_t esi,
    uint32_t edx, uint32_t ecx, uint32_t ebx, uint32_t eax,
    uint32_t ebp
);

static void test_usermode();

struct process_t {
    uint8_t* kernel_stack;
    uint8_t* user_stack;
    Pagedir* pagedir;
    
    uint32_t kernel_esp;

    uint32_t esp;
    uint32_t eflags;
    uint32_t eip;
    uint32_t edi;
    uint32_t esi;
    uint32_t edx;
    uint32_t ecx;
    uint32_t ebx;
    uint32_t eax;
    uint32_t ebp;
};
process_t _process0;
process_t _process1;
process_t* _current_process = nullptr;

#define SYSCALL_HALT    0x0
#define SYSCALL_WRITE   0x1
#define SYSCALL_YIELD   0x2

static void switch_process(process_t* process) {
    assert(process->eflags & EFLAGS_IF);

    // TRACE("Switching to process %p, ESP3: 0x%X, ESP0: 0x%X", process, process->esp, process->kernel_esp);
    _current_process = process;
    GDT::set_kernel_stack((void*)process->kernel_esp);
    // VMM::switch_pagedir(process->pagedir);
    switch_to_usermode(
        process->esp, process->eflags, process->eip,
        process->edi, process->esi,
        process->edx, process->ecx, process->ebx, process->eax,
        process->ebp
    );    
}

static void syscall_yield(const isr_regs_t* regs) {
    /* save current process state */
    assert((regs->cs & 0x3) == 3);                      /* Interrupt must originate from ring3 */
    
    _current_process->kernel_esp = regs->esp + 0x1C;    /* State of kernel ESP before the pusha in isr_stub */
    _current_process->esp = regs->useresp;              /* bad if interrupt originated in kernel mode */
    _current_process->eflags = regs->eflags;
    _current_process->eip = regs->eip;
    _current_process->edi = regs->edi;
    _current_process->esi = regs->esi;
    _current_process->edx = regs->edx;
    _current_process->ecx = regs->ecx;
    _current_process->ebx = regs->ebx;
    _current_process->eax = regs->eax;
    _current_process->ebp = regs->ebp;

    /* resume next process */
    process_t* next_process = (_current_process == &_process0 ? &_process1 : &_process0);
    switch_process(next_process);
}

static void syscall_write(const char* str) {
    TRACE("<ring3>: %s", str);
}

static void syscall_handler(const isr_regs_t* regs) {
    // TRACE(
    //     "syscall:\n"
    //     "\teax: 0x%X, ebx: 0x%X\n"
    //     "\tecx: 0x%X, edx: 0x%X\n",
    //     regs->eax, regs->ebx,
    //     regs->ecx, regs->edx
    // );
    uint32_t func = regs->eax;
    uint32_t param0 = regs->ebx;
    uint32_t param1 = regs->ecx;
    uint32_t param2 = regs->edx;

    switch(func) {
        case SYSCALL_HALT:
            halt();
            break;
        case SYSCALL_WRITE:
            syscall_write((const char*)param0);
            break;
        case SYSCALL_YIELD:
            syscall_yield(regs);
            break;
        default:
            PANIC("Unhandled syscall 0x%X", func);
    }
}

static void gpf_handler(const isr_regs_t* regs) {
    PANIC("General Protection Fault at 0x%X:0x%X", regs->cs, regs->eip);
}

extern "C" void main() {
    static const kernel_params* kparams = (kernel_params*)0x500;

	TRACE("*** RastapopoulOS kernel loaded ***");
    call_ctors();

    TRACE("Initializing kernel heap");
    KHeap::init();
    KHeap::dump();

    TRACE("Initializing GDT");
    GDT::init();
    GDT::clear_iomap(0xE9);     /* Allow access to port e9 for ring3 programs */

    TRACE("Initializing IDT");
    IDT::init();
    IDT::flush();
    IDT::install_handler(13, gpf_handler);

    TRACE("Initializing PIC");
    PIC::init();

    TRACE("Initializing system timer");
    PIT::init();

	TRACE("Initializing PMM");
	PMM::init(kparams->memmap, kparams->memmap_size);
    PMM::dump();
    TRACE("%u pages total (%u bytes)", PMM::pages_total(), PMM::pages_total() * PMM::PAGE_SIZE);

    TRACE("Initializing VMM");
    VMM::init();
    TRACE("%u pages free (%u Kb)", PMM::pages_free(), (PMM::pages_total() * PMM::PAGE_SIZE) / 1024);
    TRACE("Physical memory zones:");
    PMM::dump_zones();

    IDT::install_handler(0x80, syscall_handler);
    test_usermode();
    halt();
}

static void scheduler_timer(void* args, const isr_regs_t* regs) {
    /*
        Kernel-mode, ints disabled
        current executing process in _current_process
        Yeah baby! We can't fucking switch to usermode here, because
        we're in an irq handler. And if an irq handler doesn't acknowledge an interrupt....
    */
    /* save current process state */
    assert((regs->cs & 0x3) == 3);                      /* Interrupt must originate from ring3 */
    
    _current_process->kernel_esp = regs->esp + 0x1C;    /* State of kernel ESP before the pusha in isr_stub */
    _current_process->esp = regs->useresp;              /* bad if interrupt originated in kernel mode */
    _current_process->eflags = regs->eflags;
    _current_process->eip = regs->eip;
    _current_process->edi = regs->edi;
    _current_process->esi = regs->esi;
    _current_process->edx = regs->edx;
    _current_process->ecx = regs->ecx;
    _current_process->ebx = regs->ebx;
    _current_process->eax = regs->eax;
    _current_process->ebp = regs->ebp;

    /* resume next process */
    process_t* next_process = (_current_process == &_process0 ? &_process1 : &_process0);
    switch_process(next_process);
}

static void ring3_syscall(uint32_t function, uint32_t param0 = 0, uint32_t param1 = 0, uint32_t param2 = 0) {
    asm(
        ".intel_syntax noprefix\n"
        "mov eax, [ebp+8]\n"
        "mov ebx, [ebp+12]\n"
        "mov ecx, [ebp+16]\n"
        "mov edx, [ebp+20]\n"
        "int 0x80\n"
        ::: "eax", "ebx", "ecx", "edx", "esi", "edi", "memory"
    );
}

static void process0_entry() {
    TRACE("Process0 started");

    const char chars[] = "ABCD";
    uint32_t dat_counter = 0;
    volatile uint32_t* counter = &dat_counter; /*(uint32_t*)VMM::USERSPACE_START;*/
    *counter = 0;
    for(unsigned i=0;; i++) {
        ++(*counter);
        if(*counter == sizeof(chars))
            *counter = 0;

        IO::outb(0xE9, chars[*counter]);
        for(unsigned j=0; j<(UINT32_MAX >> 12); j++) {
            asm volatile("pause");
        }
    }
    ring3_syscall(SYSCALL_HALT);
}

static void process1_entry() {
    TRACE("Process1 started");

    const char chars[] = "abcd";
    uint32_t dat_counter = 0;
    volatile uint32_t* counter = &dat_counter; /*(uint32_t*)VMM::USERSPACE_START;*/
    *counter = 0;
    for(unsigned i=0;; i++) {
        ++(*counter);
        if(*counter == sizeof(chars))
            *counter = 0;

        IO::outb(0xE9, chars[*counter]);
        for(unsigned j=0; j<(UINT32_MAX >> 12); j++) {
            asm volatile("pause");
        }
    }
    ring3_syscall(SYSCALL_HALT);
}

static void test_usermode() {
    TRACE("Testing User-mode");

    uint32_t eflags;
    read_eflags(eflags);
    eflags |= EFLAGS_IF;

    bzero(&_process0, sizeof(_process0));
    _process0.kernel_stack = (uint8_t*)kmalloc_a(4096, 4);
    _process0.user_stack = (uint8_t*)kmalloc_a(4096, 4);
    _process0.pagedir = VMM::create_pagedir();
    _process0.esp = (uint32_t)_process0.user_stack + 4095;
    _process0.eflags = eflags;
    _process0.eip = (uint32_t)process0_entry;
    _process0.kernel_esp = (uint32_t) _process0.kernel_stack + 4095;
    // VMM::switch_pagedir(_process0.pagedir);
    // VMM::map(VMM::USERSPACE_START, PMM::alloc(), VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE);

    bzero(&_process1, sizeof(_process1));
    _process1.kernel_stack = (uint8_t*)kmalloc_a(4096, 4);
    _process1.user_stack = (uint8_t*)kmalloc_a(4096, 4);
    _process1.pagedir = VMM::create_pagedir();
    _process1.esp = (uint32_t)_process1.user_stack + 4095;
    _process1.eflags = eflags;
    _process1.eip = (uint32_t)process1_entry;
    _process1.kernel_esp = (uint32_t) _process1.kernel_stack + 4095;
    // VMM::switch_pagedir(_process1.pagedir);
    // VMM::map(VMM::USERSPACE_START, PMM::alloc(), VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE);

    Timer::schedule(scheduler_timer, nullptr, 250);

    switch_process(&_process0);
}
