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
void switch_to_usermode();

extern "C"
void usermode_program();

static void run_tests();

#define SYSCALL_HALT    0x0
#define SYSCALL_WRITE   0x1

static void syscall_write(const char* str) {
    TRACE("<ring3>: %s", str);
}

static void syscall_handler(const isr_regs_t* regs) {
    TRACE("syscall: 0x%X", regs->eax);
    switch(regs->eax) {
        case SYSCALL_HALT:
            halt();
            break;
        case SYSCALL_WRITE:
            syscall_write((const char*)regs->ecx);
            break;
        default:
            PANIC("Unhandled syscall 0x%X", regs->eax);
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

    run_tests();
    halt();
}

static void do_funky_things() {
    /*
        NOTE: Interrupts aren't enabled here, so we can't be preempted
        Make it so we CAN be preempted here
    */
    char* pagedir_name = (char*)VMM::USERSPACE_START;
    TRACE("Doing funky things with %s ...", pagedir_name);

    if(!kmalloc(65536)) {
        TRACE("There you go, exhausting kernel VA space");
    }
}

static Pagedir* p0;
static Pagedir* p1;
static Pagedir* current_pagedir;

static void schedule(void* args) {
    do_funky_things();

    if(current_pagedir == p0)
        current_pagedir = p1;
    else
        current_pagedir = p0;
    VMM::switch_pagedir(current_pagedir);
}

static void run_tests() {
    TRACE("Testing address-space switching with an active timer");

    char* str = (char*)VMM::USERSPACE_START;
    p0 = VMM::create_pagedir();
    p0->map(str, PMM::alloc(), VMM::PAGE_WRITABLE|VMM::PAGE_PRESENT);
    
    p1 = VMM::create_pagedir();
    p1->map(str, PMM::alloc(), VMM::PAGE_WRITABLE|VMM::PAGE_PRESENT);

    VMM::switch_pagedir(p0);
    strcpy(str, "pagedir p0");

    VMM::switch_pagedir(p1);
    strcpy(str, "pagedir p1");

    VMM::switch_pagedir(p0);
    current_pagedir = p0;

    Timer::schedule(schedule, nullptr, 250);

    sti();
    while(true) {
        yield();
    }
}













