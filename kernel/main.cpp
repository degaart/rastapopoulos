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
#include "process.h"

extern "C"
void switch_to_usermode();

extern "C"
void usermode_program();

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

    TRACE("Testing context switching");
    IDT::install_handler(0x80, syscall_handler);
    IDT::install_handler(13, gpf_handler);

    /*
        Testing adress-space switching
        We assume kernel-space is 0x0 - 0x3FFFFF and 0xC0000000 - 0xFFFFFFFF
        And 0x00100000 - KERNEL_END is identity-mapped

        Procedure:
            - Create new pagedir -> dir0
            - Copy current kernel pages into new pagedir
            - Map TEST_ADDRESS to page frame
            - Switch to this pagedir
            - Write some data into TEST_ADDRESS

            - Create another pagedir -> dir1
            - Copy current kernel pages into new pagedir
            - Map TEST_ADDRESS to new pageframe
            - Switch to this pagedir
            - Write some data into TEST_ADDRESS

            - Switch to dir0
            - Written data at TEST_ADDRESS should not have changed
    */
    static const uint32_t TEST_ADDRESS = 0x400000;
    Pagedir* dir0 = VMM::create_pagedir();
    uint32_t frame0 = PMM::alloc();
    dir0->map(TEST_ADDRESS, frame0, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE);
    VMM::switch_pagedir(dir0);

    char* p0 = (char*)TEST_ADDRESS;
    char* p1 = p0 + 4096;
    strcpy(p0, "Pagedir #0");

    Pagedir* dir1 = VMM::create_pagedir();
    uint32_t frame1 = PMM::alloc();
    dir1->map(TEST_ADDRESS, frame1, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE);
    dir1->map(TEST_ADDRESS + 4096, frame0, VMM::PAGE_PRESENT);

    VMM::switch_pagedir(dir1);
    strcpy(p0, "Pagedir #1");

    TRACE("frame0: %s", p1);
    TRACE("frame1: %s", p0);

    VMM::switch_pagedir(dir0);
    TRACE("frame0: %s", p0);

#if 0
    Process process(1);
    process.execute();
    halt();

    uint8_t* esp;
    read_esp(esp);
    TRACE("ESP before entering user-mode: %p", esp);
    GDT::set_kernel_stack(esp-4);                           /* Take into account stack layout when calling usermode_program() */
    switch_to_usermode();
    TRACE("Entered user-mode. ESP: %p. Executing user program", esp); /* This only works here because port e9 access is permitted by qemu and bochs on all privilege levels */

    /*
        So, when we call this function, the stack pointer points to ESP before entering it. So it doesn't work
        cause we don't fucking have the correct return address
    */
    usermode_program();
    TRACE("After executing user program. ESP: %p", esp);

    // sti();
    // while(1) {
    //     yield();
    // }
#endif

    halt();
}

