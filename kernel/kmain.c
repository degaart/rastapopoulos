#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "kernel.h"
#include "io.h"
#include "halt.h"
#include "debug.h"
#include "reboot.h"
#include "string.h"
#include "registers.h"
#include "gdt.h"
#include "idt.h"
#include "kmalloc.h"
#include "multiboot.h"
#include "pmm.h"
#include "bitset.h"
#include "vmm.h"

static void int80_handler(const struct isr_regs* regs)
{
    trace("Hello from int80");
}

extern uint32_t initial_pagedir[];
void kmain(void* multiboot_info)
{
    trace("");
    trace("*** Started ***");

    gdt_init();
    idt_init();

    trace("    .text    %p - %p", TEXT_START, TEXT_END);
    trace("    .rodata  %p - %p", RODATA_START, RODATA_END);
    trace("    .data    %p - %p", DATA_START, DATA_END);
    trace("    .bss     %p - %p", BSS_START, BSS_END);

    kmalloc_init(KERNEL_END);
    test_kmalloc();

    /*
     * Check that memory at 0xC0000000 is the same as that at 0x00000000
     */
    for(unsigned long* ptr = (unsigned long*)0xC0000000; ptr < (unsigned long*)0xC000F000; ptr++) {
        unsigned long* counterpart = (unsigned long*)((unsigned char*)ptr - 0xC0000000);
        assert2(*ptr == *counterpart, "%p != %p", ptr, counterpart);
    }

    /* And 0xFFFFF000 points to initial_pagedir */
    trace("initial_pagedir: %p", initial_pagedir);
    trace("0xFFFFF000: %p", *((unsigned long*)0xFFFFF000));

    trace("multiboot_info: 0x%X", multiboot_info);
    multiboot_init((const struct multiboot_info*)multiboot_info);

    int mmap_count;
    const struct multiboot_mmap_entry* mmap = multiboot_get_mmap(&mmap_count);
    for(int i = 0; i < mmap_count; i++) {
        trace("mmap[%d]: 0x%llX-0x%llX 0x%llX 0x%X",
              i,
              mmap[i].addr,
              mmap[i].addr + mmap[i].len - 1,
              mmap[i].len,
              mmap[i].type);
    }

    test_bitset();

    pmm_init(mmap, mmap_count);
    test_pmm();

    /*
     * Mark all kernel memory as reserved
     */
    for(unsigned long page = (unsigned long)KERNEL_START;
        page < ALIGN((unsigned long)kmalloc_brk(), PAGE_SIZE);
        page += PAGE_SIZE) {

        pmm_reserve(page);
    }
    trace("Kernel break: %p", kmalloc_brk());

    vmm_init();
    trace("Kernel area: %p - %p", KERNEL_START, kmalloc_brk());

    test_vmm();


#if 0
    for(int i = 0; i < 100; i++) {
        trace("Allocating %p pages (order %d)", PAGE_SIZE * i, i);
        unsigned char* ptr = kmalloc(PAGE_SIZE * i);
        bzero(ptr, PAGE_SIZE * i);
        kfree(ptr);
    }
#endif

    trace("*** Stopped ***");
    reboot();
}


