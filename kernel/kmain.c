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
#include "pic.h"
#include "pit.h"
#include "initrd.h"

static void int80_handler(const struct isr_regs* regs)
{
    trace("Hello from int80");
}

static
void keyboard_handler(int irq, const struct isr_regs* regs)
{
    trace("keyboard_handler");
}

extern uint32_t initial_pagedir[];
void kmain(const struct multiboot_info* multiboot_info)
{
    trace("");
    trace("*** Started ***");

    gdt_init();
    idt_init();

    trace("    .text    %p - %p", TEXT_START, TEXT_END);
    trace("    .rodata  %p - %p", RODATA_START, RODATA_END);
    trace("    .data    %p - %p", DATA_START, DATA_END);
    trace("    .bss     %p - %p", BSS_START, BSS_END);

    const unsigned char* multiboot_end = multiboot_init(multiboot_info);
    trace("Multiboot end: %p", multiboot_end);

    kmalloc_init(multiboot_end + sizeof(uint32_t));
    test_kmalloc();

    multiboot_fix(multiboot_info);

    /* And 0xFFFFF000 points to initial_pagedir */
    trace("initial_pagedir: %p", initial_pagedir);
    trace("0xFFFFF000: %p", *((unsigned long*)0xFFFFF000));

    //trace("multiboot_info: 0x%X", multiboot_info);

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
    for(unsigned long page = (unsigned long)KERNEL_START - KERNEL_BASE;
        page < ALIGN((unsigned long)kmalloc_brk() - KERNEL_BASE, PAGE_SIZE);
        page += PAGE_SIZE) {

        pmm_reserve(page);
    }
    trace("Kernel break: %p", kmalloc_brk());

    /* TODO: Mark initial modules storage as free */

    vmm_init();
    trace("Kernel area: %p - %p", KERNEL_START, kmalloc_brk());

    /*
     * Now, remap each sections of kernel with appropriate permissions
     */
    for(unsigned char* page = TEXT_START; page < TEXT_END; page += PAGE_SIZE) {
        vmm_remap(page, 0);
    }
    for(unsigned char* page = RODATA_START; page < RODATA_END; page += PAGE_SIZE) {
        vmm_remap(page, 0);
    }
    for(unsigned char* page = USER_START; page < USER_END; page += PAGE_SIZE) {
        vmm_remap(page, VMM_PAGE_USER);
    }

    /* This should throw a page fault */
    // ((char*)"aaa")[0] = '-';
    // *((unsigned char*)0xC0100000) = '-';
    // while(1);

    test_vmm();

    trace("Initializing pic");
    pic_init();

    trace("Initializing pit");
    pit_init();

    size_t initrd_size;
    const void* initrd_data = multiboot_get_initrd(&initrd_size);
    if(initrd_data) {
        trace("Loading initrd");
        initrd_init(initrd_data, initrd_size);
    }

#if 0
    trace("Entering usermode");
    unsigned char* userstack = kmalloc_aligned(PAGE_SIZE, PAGE_SIZE);
    vmm_remap(userstack, VMM_PAGE_USER);
    switch_to_usermode(userstack + PAGE_SIZE - sizeof(uint32_t));
    trace("Here????");
#endif

    trace("*** Stopped ***");
    reboot();
}


