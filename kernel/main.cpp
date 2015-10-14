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

extern "C"
void switch_to_usermode();

extern "C"
void usermode_program();

static void syscall_handler(const isr_regs_t* regs) {
    TRACE("Inside syscall handler");
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

    halt();
}

