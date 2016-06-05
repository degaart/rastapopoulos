#pragma once

typedef struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    union {
        struct {
            uint32_t tabsize;
            uint32_t strsize;
            uint32_t addr;
            uint32_t reserved;
        } sym1;

        struct {
            uint32_t num;
            uint32_t size;
            uint32_t addr;
            uint32_t shndx;
        } sym2;
    };
    uint32_t mmap_len;
    uint32_t mmap_addr;
    uint32_t drives_len;
    uint32_t drivers_addr;
    uint32_t config_table;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint32_t vbe_mode;
    uint32_t vbe_interface_seg;
    uint32_t vbe_interface_off;
    uint32_t vbe_interface_len;
} multiboot_info_t;

#define MULTIBOOT_FLAG_MEMINFO  (1 << 0)
#define MULTIBOOT_FLAG_MODINFO  (1 << 3)
#define MULTIBOOT_FLAG_SYMBOLS1 (1 << 4)
#define MULTIBOOT_FLAG_SYMBOLS2 (1 << 5)
#define MULTIBOOT_FLAG_MMAP     (1 << 6)

