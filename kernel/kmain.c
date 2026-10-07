#include "../librastaldr/multiboot.h"
#include "kernel.h"
#include "vga.h"
#include <string.h>

#define STB_SPRINTF_NOFLOAT
#define STB_SPRINTF_IMPLEMENTATION
#include <stb/stb_sprintf.h>

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
    vga_init();
    printf("RastapopoulOS kernel running\n");
    if (mb_magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        printf("PANIC: Unsupported bootloader\n");
        return;
    }

    dump_multiboot(mb_info);
}

