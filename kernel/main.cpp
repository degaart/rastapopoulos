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
#include "process.h"
#include "syscall.h"
#include "initrd.h"
#include "multiboot.h"

static void test_usermode();
static void test_vga();

static void gpf_handler(isr_regs_t* regs) {
    PANIC("General Protection Fault at 0x%X:0x%X", regs->cs, regs->eip);
}

extern "C" void main(struct multiboot_info* multiboot_info) {
    static const kernel_params* kparams = (kernel_params*)KERNEL_PARAMS;

	TRACE("*** RastapopoulOS kernel loaded ***");
    // call_ctors();

    TRACE("Multiboot INFO (0x%X):", multiboot_info->flags);
    if(multiboot_info->flags & MULTIBOOT_INFO_BOOT_LOADER_NAME) {
        TRACE("\tLoader: %s", (char*)multiboot_info->boot_loader_name);
    }
    if(multiboot_info->flags & MULTIBOOT_INFO_MODS) {
        TRACE("\tModules:");

        multiboot_mod_list* mods = (multiboot_mod_list*)multiboot_info->mods_addr;
        for(unsigned i = 0; i < multiboot_info->mods_count; i++) {
            TRACE("\t\t%u 0x%X - 0x%X %s", i, mods[i].mod_start, mods[i].mod_end, (char*)mods[i].cmdline);
        }
    }
    if(multiboot_info->flags & MULTIBOOT_INFO_MEM_MAP) {
        TRACE("\tMemmap:");

        multiboot_mmap_entry* mmap = (multiboot_mmap_entry*)multiboot_info->mmap_addr;
        for(unsigned i = 0; i < multiboot_info->mmap_length / sizeof(multiboot_mmap_entry); i++) {
            TRACE("\t\t0x%X - 0x%X %s", (uint32_t)mmap[i].addr, (uint32_t)(mmap[i].addr + mmap[i].len), mmap[i].type == MULTIBOOT_MEMORY_AVAILABLE ? "avail" : "reserved");
        }
    }
    halt();

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

    uint32_t kparams_page = truncate((uint32_t)kparams, VMM::PAGE_SIZE);
    PMM::reserve(kparams_page);
    VMM::map(kparams_page, kparams_page, VMM::PAGE_PRESENT);

    TRACE("Initializing syscall handler");
    Syscall::init();

    TRACE("Initializing process manager");
    Process::init();

    test_vga();
    // test_usermode();
    TRACE("Tests done. Halting");
    halt();
}

static void test_usermode() {
    TRACE("Testing Process manager");

    Process* p0 = Process::create("Process #0");
    p0->load_image("HELLO.BIN");

    Process* p1 = Process::create("Process #1");
    p1->load_image("HELLO.BIN");

    Process::switch_process(p0);
}

static void test_vga() {
    TRACE("Testing vgadrv");

    Process* hello_proc = Process::create("HELLO.BIN");
    hello_proc->load_image("HELLO.BIN");

    Process* proc = Process::create("VGADRV.BIN");
    proc->load_image("VGADRV.BIN");
    Process::switch_process(proc);
    
    TRACE("Tests done");
}

