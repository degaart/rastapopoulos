#include "pmm.h"
#include "../librastaldr/stdio.h"
#include "bitset.h"
#include "early_malloc.h"
#include "kernel.h"
#include <freebsd/queue.h>
#include <stddef.h>
#include <stdint.h>

#define PMM_ADDRESS_LIMIT 0xffffffffull

struct Region
{
    void* start;
    size_t len;
    Bitset bitset;
    STAILQ_ENTRY(Region) node;
};

static STAILQ_HEAD(RegionHead,
                   Region) regions = STAILQ_HEAD_INITIALIZER(regions);

static uint64_t region_start(const struct Region* region)
{
    return (uint64_t)(uintptr_t)region->start;
}

static uint64_t region_end(const struct Region* region)
{
    return region_start(region) + (uint64_t)region->len;
}

void pmm_init(const struct multiboot_mmap_entry* mmap, size_t mmap_len)
{
    if (mmap_len % sizeof(*mmap) != 0)
        panic("Invalid memory-map length");

    if (mmap_len != 0 && mmap == NULL)
        panic("NULL memory map");

    /* Release existing metadata when reinitializing. */
    struct Region* region;
    while ((region = STAILQ_FIRST(&regions)) != NULL) {
        STAILQ_REMOVE_HEAD(&regions, node);
        early_free(region);
    }

    for (const struct multiboot_mmap_entry* entry = mmap;
         entry < (struct multiboot_mmap_entry*)((void*)mmap + mmap_len);
         entry = (struct multiboot_mmap_entry*)((void*)entry + entry->size)) {
        if (entry->type != MULTIBOOT_MEMORY_AVAILABLE)
            continue;
        if (entry->len == 0 || entry->addr >= PMM_ADDRESS_LIMIT)
            continue;

        /* Clip without computing a potentially overflowing start + len */
        uint64_t len = ALIGN_DOWN(entry->len, PMM_FRAME_SIZE);
        if (len > PMM_ADDRESS_LIMIT - entry->addr)
            len = PMM_ADDRESS_LIMIT - entry->addr;

        region = early_malloc(sizeof(struct Region));
        if (!region)
            panic("Out of memory");

        region->start = (void*)(uintptr_t)entry->addr;
        region->len = (size_t)len;

        size_t bitcount = len / PMM_FRAME_SIZE;
        size_t bitset_size = bitset_storage_size(bitcount);
        uint32_t* storage = early_malloc(bitset_size);
        if (!storage)
            panic("Out of memory");
        bitset_init(&region->bitset, storage, bitcount);

        struct Region* previous = NULL;
        struct Region* current = STAILQ_FIRST(&regions);
        while (current != NULL && region_start(current) < entry->addr) {
            previous = current;
            current = STAILQ_NEXT(current, node);
        }

        if (previous == NULL)
            STAILQ_INSERT_HEAD(&regions, region, node);
        else
            STAILQ_INSERT_AFTER(&regions, previous, region, node);
    }

    /* Coalesce overlapping and adjacent regions */
    region = STAILQ_FIRST(&regions);
    while (region != NULL) {
        struct Region* next = STAILQ_NEXT(region, node);
        uint64_t end;
        uint64_t next_end;

        if (!next)
            break;

        end = region_end(region);
        if (region_start(next) > end) {
            region = next;
            continue;
        }

        next_end = region_end(next);
        if (next_end > end)
            region->len = (size_t)(next_end - region_start(region));

        STAILQ_REMOVE(&regions, next, Region, node);
        early_free(next);
    }

    /* Force page0 to be allocated */
    region = STAILQ_FIRST(&regions);
    if (region->start != 0)
        panic("Logic error");
    if (!bitset_set(&region->bitset, 0))
        panic("Logic error");
}

void pmm_dump(void)
{
    printf("PMM regions:\n");

    struct Region* region;
    STAILQ_FOREACH(region, &regions, node)
    {
        printf("    %p-%p 0x%lx\n", region->start,
               region->start + region->len - 1, region->len);
    }
}

void* pmm_alloc(void)
{
    struct Region* region;
    STAILQ_FOREACH(region, &regions, node)
    {
        size_t bit = bitset_find(&region->bitset);
        if (bit == SIZE_MAX)
            continue;
        if (!bitset_set(&region->bitset, bit))
            panic("Logic error");
        return region->start + (bit * PMM_FRAME_SIZE);
    }
    return NULL;
}

void pmm_free(void* frame)
{
    if ((uintptr_t)frame & (PMM_FRAME_SIZE - 1))
        panic("Invalid frame: 0x%p", frame);

    int found = 0;
    struct Region* region;
    STAILQ_FOREACH(region, &regions, node)
    {
        if (frame >= region->start && frame < region->start + region->len) {
            size_t bit = (uintptr_t)(frame - region->start) / PMM_FRAME_SIZE;
            if (!bitset_unset(&region->bitset, bit))
                panic("Logic error");
            found++;
        }
    }

    if (found == 0)
        panic("Frame 0x%p was already free", frame);
    else if (found > 1)
        panic("Frame 0x%p was found in multiple regions", frame);
}

size_t pmm_info(void)
{
    size_t result = 0;
    struct Region* region;
    STAILQ_FOREACH(region, &regions, node)
    {
        result += bitset_count_unset(&region->bitset);
    }
    return result * PMM_FRAME_SIZE;
}

