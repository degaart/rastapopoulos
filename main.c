#include "debug.h"
#include "gdt.h"
#include "string.h"
#include "util.h"
#include <stddef.h>

void test_idt();

void kmain(const void* multiboot_info, uint32_t multiboot_magic)
{
    trace_init();
    if(multiboot_magic != 0x2BADB002) {
        TRACE("PANIC: Bad multiboot magic");
        while(1);
    }

    /* setup GDT */
    TRACE("Setting up GDT");
    gdt_init();

    /* Test IDT */
    test_idt();
    TRACE("After setting up IDT");
}

