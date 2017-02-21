#include "pmm.h"
#include "kmalloc.h"
#include "multiboot.h"
#include "debug.h"
#include "string.h"
#include "util.h"

struct memregion {
    uint32_t addr;
    uint32_t len;
    struct bitset* bitmap;
    struct memregion* next;
};

struct memregion* memregions = NULL;

static void add_region(uint32_t addr, uint32_t len)
{
    struct memregion* region = kmalloc(sizeof(struct memregion));
    region->addr = addr;
    region->len = len;
    region->next = memregions;

    memregions = region;
}

void pmm_init(const struct multiboot_info* multiboot_info)
{
    const struct multiboot_mmap_entry* mmap_entries = (const struct multiboot_mmap_entry*)multiboot_info->mmap_addr;
    uint32_t mmap_entries_count = multiboot_info->mmap_len / sizeof(struct multiboot_mmap_entry);
    for(uint32_t i = 0; i < mmap_entries_count; i++) {
        const struct multiboot_mmap_entry* entry = mmap_entries + i;
        if(!HIDWORD(entry->addr) && entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
            add_region(LODWORD(entry->addr), LODWORD(entry->len));
        }
    }
}







