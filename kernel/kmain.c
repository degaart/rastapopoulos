#include "kernel.h"
#include "io.h"
#include "string.h"
#include "debug.h"
#include "registers.h"
#include "multiboot.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "timer.h"
#include "pmm.h"
#include "util.h"
#include "kmalloc.h"
#include "vmm.h"

uint32_t KERNEL_START = (uint32_t) &_KERNEL_START_;
uint32_t KERNEL_END = (uint32_t) &_KERNEL_END_;

void reboot()
{
    trace("*** Rastapopoulos rebooted ***\n\n\n");

    uint8_t good = 0x02;
    while (good & 0x02)
        good = inb(0x64);
    outb(0x64, 0xFE);
    halt();
}

void abort()
{
    backtrace();
    reboot();
}

void halt()
{
    cli();
    hlt();
}

static void reboot_timer(void* data, const struct isr_regs* regs)
{
    trace("*** Rebooting Rastapopoulos ***\n\n\n");
    reboot();
}

static void dump_multiboot_info(const struct multiboot_info* multiboot_info)
{
    trace("Multiboot info: %p", multiboot_info);

    char multiboot_flags[32] = {};
    if(multiboot_info->flags & MULTIBOOT_FLAG_MEMINFO) {
        strlcat(multiboot_flags, "MEM ", sizeof(multiboot_flags));
        trace("Lower memory size: %dk", multiboot_info->mem_lower);
        trace("High memory size: %dk", multiboot_info->mem_upper);
    }
    if(multiboot_info->flags & MULTIBOOT_FLAG_MODINFO) {
        strlcat(multiboot_flags, "MOD ", sizeof(multiboot_flags));
        trace("Modules count: %d", multiboot_info->mods_count);
        trace("Modules load address: %p", multiboot_info->mods_addr);
    }
    if(multiboot_info->flags & MULTIBOOT_FLAG_SYMBOLS1) {
        strlcat(multiboot_flags, "SYM1 ", sizeof(multiboot_flags));
    }
    if(multiboot_info->flags & MULTIBOOT_FLAG_SYMBOLS2) {
        strlcat(multiboot_flags, "SYM2 ", sizeof(multiboot_flags));

        load_symbols(multiboot_info);
    }
    if(multiboot_info->flags & MULTIBOOT_FLAG_MMAP) {
        strlcat(multiboot_flags, "MMAP ", sizeof(multiboot_flags));
        trace("Memory map len: %d", multiboot_info->mmap_len);
        trace("Memory map load address: %p", multiboot_info->mmap_addr);
    }

    trace("Multiboot flags: %s", multiboot_flags);
    trace("_KERNEL_END_: %p", &_KERNEL_END_);
}


static void run_tests()
{
    void test_bitset();
    void test_vmm();
    void test_usermode();
    void test_kmalloc();

    test_bitset();
    test_vmm();
    test_kmalloc();
    test_usermode();
}

void kmain(const struct multiboot_info* multiboot_info)
{
    trace("*** Rastapopoulos booted ***");

    load_symbols(multiboot_info);
    
    // GDT
    gdt_init();
    gdt_iomap_set(DEBUG_PORT, 0);

    // IDT
    idt_init();
    idt_flush();

    // Physical memory manager
    pmm_init(multiboot_info);

    /*
     * Reserve specific areas of unpaged memory
     * BDA:     0x00000400 - 0x000004FF
     * EDBA:    0x0009FC00 - 0x0009FFFF
     * VGA:     0x000A0000 - 0x000FFFFF
     * multiboot_info
     * kernel code and data section
     * initial kernel heap
     */
    pmm_reserve(0x0);
    for(uint32_t page = TRUNCATE(0x9FC00, PAGE_SIZE); page < 0x9FFFF; page += PAGE_SIZE) {
        if(pmm_exists(page))
            pmm_reserve(page);
    }

    for(uint32_t page = TRUNCATE(0xA0000, PAGE_SIZE); page < 0xFFFFF; page += PAGE_SIZE) {
        if(pmm_exists(page))
            pmm_reserve(page);
    }

    for(uint32_t page = TRUNCATE((uint32_t)multiboot_info, PAGE_SIZE); 
        page < (uint32_t)multiboot_info + sizeof(struct multiboot_info); 
        page += PAGE_SIZE) {
        if(pmm_exists(page))
            pmm_reserve(page);
    }

    for(uint32_t page = TRUNCATE(KERNEL_START, PAGE_SIZE); 
        page < KERNEL_END; 
        page += PAGE_SIZE) {
        if(pmm_exists(page))
            pmm_reserve(page);
    }

    struct kernel_heap_info heap_info;
    kernel_heap_info(&heap_info);
    for(uint32_t page = TRUNCATE(heap_info.heap_start, PAGE_SIZE); 
        page < heap_info.heap_start + heap_info.heap_size; 
        page += PAGE_SIZE) {

        if(pmm_exists(page) && !pmm_reserved(page))
            pmm_reserve(page);
    }

    // Virtual memory manager
    vmm_init();

    // PIC
    pic_init();

    // System timer
    timer_init();
    timer_schedule(reboot_timer, NULL, 3000, false);

    // Run tests
    run_tests();
    reboot();
}

