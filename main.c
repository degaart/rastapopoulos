#include "debug.h"
#include "gdt.h"
#include "idt.h"
#include <stddef.h>

static void handle_int80(struct isr_regs* regs)
{
    TRACE("int 0x80 called");
}

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

    /* setup IDT */
    idt_init();
    idt_add_handler(0x80, handle_int80, IDT_DPL0);
    TRACE("After setting up IDT");
    asm volatile("int $0x80\n":::"memory");
    TRACE("After calling int 80");
}

