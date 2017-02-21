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

extern unsigned char _KERNEL_END_;   /* Set by linker */

void reboot()
{
    trace("*** Rastapopoulos rebooted ***\n\n\n");

    uint8_t good = 0x02;
    while (good & 0x02)
        good = inb(0x64);
    outb(0x64, 0xFE);
    halt();
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

void test_bitset();

static void run_tests()
{
    test_bitset();
}

void kmain(const struct multiboot_info* multiboot_info)
{
    trace("*** Rastapopoulos booted ***");
    
    // GDT
    gdt_init();

    // IDT
    idt_init();
    idt_flush();

    // Physical memory manager
    pmm_init(multiboot_info);

    // PIC
    pic_init();

    // System timer
    timer_init();
    timer_schedule(reboot_timer, NULL, 3000, false);

    // Run tests
    run_tests();
    reboot();
}

