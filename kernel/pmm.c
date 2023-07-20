#include "kmalloc.h"
#include "pmm.h"
#include <bitset.h>
#include <debug.h>
#include <string.h>
#include <util.h>

#define PAGESIZE 4096

struct zone {
    uintptr_t start;     /* start of the zone (aligned to PAGESIZE) */
    size_t size;         /* size of zone in bytes (aligned to PAGESIZE) */
    size_t frame_count;  /* number of frames in this zone */
    struct bitset* bitmap;
};

static struct zone** zones;
static size_t zone_count;

/* Returns a physical address */
uint32_t pmm_alloc()
{
    for(size_t i = 0; i < zone_count; i++) {
        size_t frame_index = bitset_find(zones[i]->bitmap);
        if(frame_index != BITSET_INVALID) {
            uint32_t result = zones[i]->start + (frame_index * PAGESIZE);
            pmm_set(result);
            return result;
        }
    }
    TRACE("Physical memory exhaustion");
    return INVALID_FRAME;
}

static void pmm_set_value(uint32_t phys_addr, bool value)
{
    assert((phys_addr & (PAGESIZE - 1)) == 0);  /* Check aligned to PAGESIZE */
    for(size_t i = 0; i < zone_count; i++) {
        if(phys_addr >= zones[i]->start &&
           phys_addr < zones[i]->start + zones[i]->size)
        {
            size_t frame_index = (phys_addr - zones[i]->start) / PAGESIZE;

            /*
             * Disallow clearing an already-cleared frame, or
             * setting an already-set frame
             */
            assert(bitset_get(zones[i]->bitmap, frame_index) != value);
            bitset_set_value(zones[i]->bitmap, frame_index, value);
            return;
        }
    }
    PANIC("Invalid frame: %p", (void*)phys_addr);
}

bool pmm_get(uint32_t phys_addr)
{
    assert((phys_addr & (PAGESIZE - 1)) == 0);  /* Check aligned to PAGESIZE */
    for(size_t i = 0; i < zone_count; i++) {
        if(phys_addr >= zones[i]->start &&
           phys_addr < zones[i]->start + zones[i]->size)
        {
            size_t frame_index = (phys_addr - zones[i]->start) / PAGESIZE;
            return bitset_get(zones[i]->bitmap, frame_index);
        }
    }
    PANIC("Invalid frame: %p", (void*)phys_addr);
    return false;
}

void pmm_set(uint32_t phys_addr)
{
    assert((phys_addr & (PAGESIZE - 1)) == 0);  /* Check aligned to PAGESIZE */
    pmm_set_value(phys_addr, true);
}

void pmm_clear(uint32_t phys_addr)
{
    assert((phys_addr & (PAGESIZE - 1)) == 0);  /* Check aligned to PAGESIZE */
    pmm_set_value(phys_addr, false);
}

static void pmm_test()
{
    /* Test the structure of the memory map */
    assert(zone_count == 2);
    assert(zones[0]->start == 0x00000000);
    assert(zones[0]->size  == 0x0009F000);
    assert(zones[0]->frame_count == 159);
    assert(zones[1]->start == 0x00101000);
    assert(zones[1]->size  == 0x003FF000);
    assert(zones[1]->frame_count == 1023);

    /* Test pmm_set / pmm_clear */
    pmm_set(0x00000000);
    assert(pmm_get(0x00000000));
    for(uint32_t frame = 0x1000; frame < 0x9F000; frame += 0x1000) {
        assert(!pmm_get(frame));
    }
    for(uint32_t frame = 0x101000; frame < 0x500000; frame += 0x1000) {
        assert(!pmm_get(frame));
    }

    pmm_set(0x00001000);
    assert(pmm_get(0x00000000));
    assert(pmm_get(0x00001000));
    for(uint32_t frame = 0x2000; frame < 0x9F000; frame += 0x1000) {
        assert(!pmm_get(frame));
    }
    for(uint32_t frame = 0x101000; frame < 0x500000; frame += 0x1000) {
        assert(!pmm_get(frame));
    }

    pmm_set(0x00101000);
    assert(pmm_get(0x00000000));
    assert(pmm_get(0x00001000));
    assert(pmm_get(0x00101000));
    for(uint32_t frame = 0x2000; frame < 0x9F000; frame += 0x1000) {
        assert(!pmm_get(frame));
    }
    for(uint32_t frame = 0x102000; frame < 0x500000; frame += 0x1000) {
        assert(!pmm_get(frame));
    }

    pmm_set(0x0009E000);
    pmm_set(0x004FF000);
    assert(pmm_get(0x00000000));
    assert(pmm_get(0x00001000));
    assert(pmm_get(0x0009E000));
    assert(pmm_get(0x00101000));
    assert(pmm_get(0x004FF000));
    for(uint32_t frame = 0x2000; frame < 0x9E000; frame += 0x1000) {
        assert(!pmm_get(frame));
    }
    for(uint32_t frame = 0x102000; frame < 0x004FF000; frame += 0x1000) {
        assert(!pmm_get(frame));
    }

    /* Test pmm_alloc */
    uint32_t frame = pmm_alloc();
    assert(frame == 0x2000);

    for(uint32_t frame = 0x2000; frame < 0x9E000; frame += 0x1000) {
        pmm_set(frame);
    }
    for(uint32_t frame = 0x102000; frame < 0x004FF000; frame += 0x1000) {
        pmm_set(frame);
    }

    frame = pmm_alloc();
    assert(frame == INVALID_FRAME);
}

void pmm_init(const struct multiboot_mmap_entry* entries, size_t length)
{
#if defined(UNIT_TESTS) && defined(TEST_PMM)
    static struct multiboot_mmap_entry test_entries[] = {
        { 20, 0x00000000, 0x0009FC00, 0x00000001 },
        { 20, 0x0009FC00, 0x00000400, 0x00000002 },
        { 20, 0x000F0000, 0x00010000, 0x00000002 },
        { 20, 0x00100001, 0x00400000, 0x00000001 },
        { 20, 0x00400000, 0x00020000, 0x00000002 },
        { 20, 0xFFFC0000, 0x00040000, 0x00000002 },
    };
    entries = test_entries;
    count = sizeof(test_entries) / sizeof(test_entries[0]);
#endif

    /* Determine the count of available zones */
    zone_count = 0;
    MULTIBOOT_MMAP_ITERATE(entries, entry, length) {
        if(entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
            zone_count++;
        }
    }

    zones = early_kmalloc(sizeof(struct zone*) * zone_count);
    memset(zones, 0, sizeof(struct zone*) * zone_count);

    size_t i = 0;  /* zone index */
    MULTIBOOT_MMAP_ITERATE(entries, entry, length) {
        if(entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
            uint32_t start = entry->addr & 0xFFFFFFFF;
            uint32_t size = entry->len & 0xFFFFFFFF;
            uint32_t aligned_start = ALIGN(start, PAGESIZE);
            uint32_t aligned_size = ROUND(size - (aligned_start - start), PAGESIZE);
            size_t frame_count = aligned_size / PAGESIZE;

            size_t bitmap_size = bitset_get_size(frame_count);
            zones[i] = early_kmalloc(sizeof(struct zone));
            zones[i]->start = aligned_start;
            zones[i]->size = aligned_size;
            zones[i]->frame_count = frame_count;
            zones[i]->bitmap = early_kmalloc(bitmap_size);
            bitset_init(zones[i]->bitmap, bitmap_size, frame_count);
            i++;
        }
    }


    void bitset_test();
    ADD_TEST(bitset_test);

#if defined(UNIT_TESTS) && defined(TEST_PMM)
    ADD_TEST(pmm_test);
#endif

}

