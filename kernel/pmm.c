#include "pmm.h"
#include "kmalloc.h"
#include "multiboot.h"
#include "debug.h"
#include "string.h"
#include "util.h"
#include "bitset.h"
#include "kernel.h"

struct memregion {
    uint32_t addr;
    uint32_t len;
    struct bitset* bitmap;
    struct memregion* next;
};

static bool initialized = false;
struct memregion* memregions = NULL;

static void add_region(uint32_t addr, uint32_t len)
{
    uint32_t aligned_start = ALIGN(addr, PAGE_SIZE);
    len -= aligned_start - addr;

    uint32_t aligned_len = TRUNCATE(len, PAGE_SIZE);

    assert(!(aligned_start % PAGE_SIZE));
    assert(!(aligned_len % PAGE_SIZE));

    struct memregion* region = kmalloc(sizeof(struct memregion));
    region->addr = aligned_start;
    region->len = aligned_len;
    region->next = memregions;
    region->bitmap = bitset_new(aligned_len / PAGE_SIZE);

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
    initialized = true;
}

bool pmm_initialized()
{
    return initialized;
}

void pmm_reserve(uint32_t page)
{
    assert(!(page % PAGE_SIZE));

    for(struct memregion* region = memregions; region; region = region->next) {
        if(page >= region->addr && page < region->addr + region->len) {
            uint32_t offset = page - region->addr;
            uint32_t index = offset / PAGE_SIZE;

            if(bitset_test(region->bitmap, index)) {
                trace("Error: page %p already reserved!", page);
                abort();
            }
            bitset_set(region->bitmap, index);
        }
    }
}

bool pmm_reserved(uint32_t page)
{
    assert(!(page % PAGE_SIZE));

    bool result = true;
    for(struct memregion* region = memregions; region; region = region->next) {
        if(page >= region->addr && page < region->addr + region->len) {
            uint32_t offset = page - region->addr;
            uint32_t index = offset / PAGE_SIZE;

            result = bitset_test(region->bitmap, index);
            break;
        }
    }

    return result;
}

uint32_t pmm_alloc()
{
    for(struct memregion* region = memregions; region; region = region->next) {
        uint32_t index = bitset_find(region->bitmap, 0);
        if(index != BITSET_INVALID_INDEX) {
            bitset_set(region->bitmap, index);
            uint32_t page = region->addr + (index * PAGE_SIZE);
            return page;
        }
    }
    return PMM_INVALID_PAGE;
}


