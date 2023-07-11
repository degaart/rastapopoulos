#include "gdt.h"
#include "idt.h"
#include <debug.h>
#include <serial.h>
#include <string.h>
#include <util.h>
#include <vga.h>

#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

static void handle_int80(struct isr_regs* regs)
{
    TRACE("int 0x80 called");
}

void trace_init()
{
    serial_write_char('\n');
}

bool debug_write(char ch, void*)
{
    serial_write_char(ch);
    vga_write_char(ch, COLOR_LIGHTGRAY);
    return true;
}

void trace(const char* file, int line, const char* fn, const char* fmt, ...)
{
    char prefix[64];
    snprintf(prefix, sizeof(prefix), "[%s:%s:%d] ", basename(file), fn, line);
    serial_write_string(prefix);
    vga_write_string(prefix, COLOR_LIGHTGRAY);

    va_list args;
    va_start(args, fmt);
    formatv(debug_write, NULL, fmt, args);
    va_end(args);
    
    debug_write('\n', NULL);
}

void panic(const char* file, int line, const char* fn, const char* fmt, ...)
{
    trace(file, line, fn, "*** KERNEL PANIC ***");
    va_list args;
    va_start(args, fmt);
    formatv(debug_write, NULL, fmt, args);
    va_end(args);
    HALT();
}

void kmain(const void* multiboot_info, uint32_t multiboot_magic)
{
    vga_init();
    trace_init();

    if(multiboot_magic != 0x2BADB002) {
        TRACE("PANIC: Bad multiboot magic");
    }

    /* setup GDT */
    TRACE("Setting up GDT");
    gdt_init();

    /* setup IDT */
    idt_init();
    idt_add_handler(0x80, handle_int80, IDT_DPL0);
    TRACE("After setting up IDT");
    asm volatile("int 0x80\n":::"memory");
    TRACE("After calling int 80");
}

