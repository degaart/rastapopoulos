#include "early_malloc.h"
#include "idt.h"
#include "kernel.h"
#include "pmm.h"
#include "vga.h"
#include <gdt.h>
#include <multiboot.h>
#include <string.h>

#define STB_SPRINTF_NOFLOAT
#define STB_SPRINTF_IMPLEMENTATION
#include <stb/stb_sprintf.h>

#define debugbreak() asm volatile("xchg bx, bx" ::: "memory")
extern void halt(void) __attribute__((noreturn));

static char* sprintfcb(const char* buf, void* user, int len)
{
    vga_write(buf, len, VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    return (char*)buf;
}

__attribute__((format(printf, 1, 2))) int printf(const char* fmt, ...)
{
    char buf[STB_SPRINTF_MIN];

    va_list args;
    va_start(args, fmt);
    int ret = stbsp_vsprintfcb(sprintfcb, NULL, buf, fmt, args);
    va_end(args);
    return ret;
}

void __panic(const char* file, int line, const char* fmt, ...)
{
    printf("PANIC at %s:%d -- ", file, line);

    char buf[STB_SPRINTF_MIN];
    va_list args;
    va_start(args, fmt);
    stbsp_vsprintfcb(sprintfcb, NULL, buf, fmt, args);
    va_end(args);

    halt();
}

static void dump_multiboot(const struct multiboot_info* info)
{
    printf("Multiboot info:\n");
    if (info->flags & MULTIBOOT_INFO_MEMORY) {
        printf("    mem_lower: 0x%lx, mem_upper: 0x%lx\n", info->mem_lower,
               info->mem_upper);
    }
    if (info->flags & MULTIBOOT_INFO_BOOTDEV) {
        printf("    boot_device: 0x%lx\n", info->boot_device);
    }
    if (info->flags & MULTIBOOT_INFO_MEM_MAP) {
        const struct multiboot_mmap_entry* entry =
            (struct multiboot_mmap_entry*)info->mmap_addr;
        for (; entry < (struct multiboot_mmap_entry*)(info->mmap_addr +
                                                      info->mmap_length);
             entry =
                 (struct multiboot_mmap_entry*)((void*)entry + entry->size)) {
            const char* type = NULL;
            switch (entry->type) {
            case MULTIBOOT_MEMORY_AVAILABLE:
                type = "AVL";
                break;
            case MULTIBOOT_MEMORY_RESERVED:
                type = "RSVD";
                break;
            case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE:
                type = "ACPI";
                break;
            case MULTIBOOT_MEMORY_NVS:
                type = "NVS";
                break;
            case MULTIBOOT_MEMORY_BADRAM:
                type = "BAD";
                break;
            default:
                type = "UNK";
                break;
            }

            printf("    mmap: 0x%08llx-0x%08llx 0x%08llx %s\n", entry->addr,
                   entry->addr + entry->len - 1, entry->len, type);
        }
    }
    if (info->flags & MULTIBOOT_INFO_BOOT_LOADER_NAME) {
        printf("    bootloader: %s\n", (char*)info->boot_loader_name);
    }
}

void kmain(uint32_t mb_magic, const struct multiboot_info* mb_info)
{
    early_malloc_init();

    vga_init();
    printf("RastapopoulOS kernel running\n");
    if (mb_magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        printf("PANIC: Unsupported bootloader\n");
        return;
    }
    dump_multiboot(mb_info);

    struct Gdt gdt = {0};
    gdt_set_entry(&gdt.null, 0, 0, 0, 0);
    gdt_set_entry(&gdt.code, 0, 0xfffff, GDT_ACCESS_CODE, GDT_FLAGS_32BIT_4K);
    gdt_set_entry(&gdt.data, 0, 0xfffff, GDT_ACCESS_DATA, GDT_FLAGS_32BIT_4K);
    gdt_set_entry(&gdt.usercode, 0, 0xfffff, GDT_ACCESS_USERCODE,
                  GDT_FLAGS_32BIT_4K);
    gdt_set_entry(&gdt.userdata, 0, 0xfffff, GDT_ACCESS_USERDATA,
                  GDT_FLAGS_32BIT_4K);

    struct Gdtr gdtr = {.limit = sizeof(gdt) - 1,
                        .base = (uint32_t)(uintptr_t)&gdt};
    gdt_load(&gdtr);
    idt_init();

    struct multiboot_mmap_entry* mmap_entries =
        (struct multiboot_mmap_entry*)mb_info->mmap_addr;
    size_t mmap_len = mb_info->mmap_length;
    if (mmap_len == 0) {
        mmap_len = sizeof(struct multiboot_mmap_entry) * 2;
        mmap_entries = early_malloc(mmap_len);

        mmap_entries[0].size = sizeof(struct multiboot_mmap_entry);
        mmap_entries[0].addr = 0;
        mmap_entries[0].len = mb_info->mem_lower * 1024;
        mmap_entries[0].type = MULTIBOOT_MEMORY_AVAILABLE;

        mmap_entries[1].size = sizeof(struct multiboot_mmap_entry);
        mmap_entries[1].addr = 0x100000; /* 1Mb */
        mmap_entries[1].len = mb_info->mem_upper * 1024;
        mmap_entries[1].type = MULTIBOOT_MEMORY_AVAILABLE;
    }
    pmm_init(mmap_entries, mmap_len);
    pmm_dump();
    printf("Free memory: %zu bytes\n", pmm_info());

    void* frame = pmm_alloc();
    if (!frame)
        panic("Failed to allocate frame");
    printf("Frame: %p\n", frame);
    printf("Free memory: %zu bytes\n", pmm_info());
    pmm_free(frame);
    printf("Free memory: %zu bytes\n", pmm_info());

    void* new_frame = pmm_alloc();
    if (new_frame != frame)
        panic("Error in pmm implementation");
    printf("Free memory: %zu bytes\n", pmm_info());
    pmm_free(new_frame);

    printf("Free memory: %zu bytes\n", pmm_info());
    printf("OK\n");
}

