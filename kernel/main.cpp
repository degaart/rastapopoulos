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

extern "C"
void switch_to_usermode();

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

    void* esp;
    read_esp(esp);
    TRACE("ESP before entering user-mode: %p", esp);
    GDT::set_kernel_stack(esp);

    switch_to_usermode();
    BREAKPOINT();
    asm(
        ".intel_syntax noprefix\n"
        "int 0x80\n"
    );

    TRACE("Outside of syscall handler. ESP: %p", esp);

    // sti();
    // while(1) {
    //     yield();
    // }

    halt();
}

