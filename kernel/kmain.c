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

static void int80_handler(const struct isr_regs* regs)
{
    trace("Hello from int80");
}

void kmain(void* multiboot_info)
{
    trace("");
    trace("*** Started ***");

    gdt_init();
    idt_init();

    trace("KERNEL_START: 0x%X", KERNEL_START);
    trace("KERNEL_END: 0x%X", KERNEL_END);

    kmalloc_init(KERNEL_END);

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

    pmm_init(mmap, mmap_count);
    test_pmm();

    test_bitset();

    trace("*** Stopped ***");
    reboot();
}


