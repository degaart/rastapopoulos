#include "gdt.h"
#include "idt.h"
#include <debug.h>
#include <multiboot.h>
#include <serial.h>
#include <string.h>
#include <util.h>
#include <vga.h>

#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

extern unsigned char _heap_start;
static unsigned char* _heap = &_heap_start;
static struct multiboot_info multiboot_info;

static void* early_kmalloc(size_t size)
{
    unsigned char* result = (unsigned char*)ALIGN((uintptr_t)_heap, 16);
    _heap += size;
    return result;
}

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

void kmain(const struct multiboot_info* multiboot, uint32_t multiboot_magic)
{
    vga_init();
    trace_init();
    gdt_init();

    /* Save multiboot information elsewhere before we manage to overwrite it */
    if(multiboot_magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        TRACE("PANIC: Bad multiboot magic");
    }
    memcpy(&multiboot_info, multiboot, sizeof(multiboot_info));
    if(multiboot_info.flags & MULTIBOOT_INFO_MEM_MAP) {
        struct multiboot_mmap_entry* mmap_entries = early_kmalloc(multiboot_info.mmap_length);
        memcpy(mmap_entries,
               multiboot_info.mmap_addr,
               multiboot_info.mmap_length);
        multiboot_info.mmap_addr = mmap_entries;
    }

    /*
     * Display memory map
     * multiboot_mmap_entry->size does not include the `size` member,
     * so we must add the size of an uint32_t while skipping to the next
     * entry
     */
    TRACE("Memory map:");
    TRACE("     ADDR       LEN        TYPE");
    for(const struct multiboot_mmap_entry* e = multiboot_info.mmap_addr;
        (uintptr_t)e < (uintptr_t)multiboot_info.mmap_addr + multiboot_info.mmap_length;
        e = (const struct multiboot_mmap_entry*)((uintptr_t)e + e->size + sizeof(uint32_t)))
    {
        uint32_t addr = e->addr & 0xFFFFFFFF;
        uint32_t len = e->len & 0xFFFFFFFF;
        TRACE("    %p %p %p", addr, len, e->type);
    }

    /* setup IDT */
    idt_init();
    idt_add_handler(0x80, handle_int80, IDT_DPL0);
    TRACE("After setting up IDT");
    asm volatile("int 0x80\n":::"memory");
    TRACE("After calling int 80");
}

