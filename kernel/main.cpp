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

static void int80_handler(const isr_regs_t* regs) {
    TRACE("INT80 called");
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
    GDT::dump();
    GDT::flush();

    TRACE("Initializing IDT");
    IDT::init();
    IDT::flush();
    IDT::install_handler(0x80, int80_handler);

	TRACE("Initializing PMM");
	PMM::init(kparams->memmap, kparams->memmap_size);
    PMM::dump();
    TRACE("%u pages total (%u bytes)", PMM::pages_total(), PMM::pages_total() * PMM::PAGE_SIZE);

    TRACE("Initializing VMM");
    VMM::init();
    TRACE("%u pages free (%u Kb)", PMM::pages_free(), (PMM::pages_total() * PMM::PAGE_SIZE) / 1024);
    TRACE("Physical memory zones:");
    PMM::dump_zones();

    TRACE("Testing VMM::map()");
    uint8_t *p0 = (uint8_t*)0x400000;            /* 4 MB in, guaranteed to not be mapped at this point */
    VMM::map(p0, 0xB8000, VMM::PAGE_PRESENT | VMM::PAGE_WRITABLE, 0);
    *p0 = 'X';

    uint8_t *p1 = (uint8_t*)(0x400000 + 4096);  /* has a pagedir but no pagetable entry */
    VMM::map(p1, 0xB8000, VMM::PAGE_PRESENT);
    assert(*p0 == *p1);
    *p0 ^= 0xCC;
    assert(*p0 == *p1);

    *p1 = 120;

    halt();
}

