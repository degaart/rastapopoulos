/*
 * Physical memory management
 * Essentially a page-frame allocator unsing a bitmap
 */
#include <stddef.h>
#include "pmm.h"
#include "queue.h"
#include "kmalloc.h"
#include "string.h"
#include "debug.h"
#include "bitset.h"

struct bitmap_node {
    unsigned long long base;
    unsigned long length;
    struct bitset* bitset;
    TAILQ_ENTRY(bitmap_node) next;
};
TAILQ_HEAD(bitmap, bitmap_node) bitmaps;

struct bitset_entry {
    struct bitset* bitset;
    size_t index;
};

static struct bitset_entry get_bitset(unsigned long page)
{
    struct bitset_entry result = { NULL, 0 };

    assert((page % PAGE_SIZE) == 0);
    struct bitmap_node* node;
    TAILQ_FOREACH(node, &bitmaps, next) {
        if(page >= node->base && page < node->base + node->length) {
            assert(page - node->base < 0x100000000);

            size_t index = (size_t)((page - node->base) / PAGE_SIZE);
            result.bitset = node->bitset;
            result.index = index;
            break;
        }
    }

    return result;
}

void pmm_init(const struct multiboot_mmap_entry* map, int count)
{
    TAILQ_INIT(&bitmaps);
    for(size_t i = 0; i < count; i++) {
        if(map[i].type == MULTIBOOT_MEMORY_AVAILABLE) {
            trace("Adding bitmap for %lld bytes at 0x%llX", map[i].len, map[i].addr);
            assert((map[i].addr % PAGE_SIZE) == 0);

            size_t len = (size_t)TRUNCATE(map[i].len, PAGE_SIZE);
            size_t count = len / PAGE_SIZE;
            struct bitmap_node* node = kmalloc(sizeof(struct bitmap_node));
            node->base = map[i].addr;
            node->length = len;
            node->bitset = bitset_alloc(count);
            TAILQ_INSERT_TAIL(&bitmaps, node, next);
        }
    }
}

void pmm_reserve_range(unsigned long page, size_t length)
{
    if(page % PAGE_SIZE)
        panic("Invalid page: %p", page);
    else if(length % PAGE_SIZE)
        panic("Invalid reservation length: 0x%X", length);

    for(unsigned long p = page; p < page + length; p += PAGE_SIZE) {
        struct bitset_entry entry = get_bitset(p);
        if(!entry.bitset)
            panic("Page not found: %p", p);
        else if(bitset_test(entry.bitset, entry.index))
            panic("Page already reserved: %p", p);
        bitset_set(entry.bitset, entry.index);
    }
}

void pmm_reserve(unsigned long page)
{
    pmm_reserve_range(page, PAGE_SIZE);
}

bool pmm_exists(unsigned long page)
{
    if(page % PAGE_SIZE)
        panic("Invalid page: %p", page);
    
    struct bitset_entry entry = get_bitset(page);
    return entry.bitset != NULL;
}

bool pmm_reserved(unsigned long page)
{
    if(page % PAGE_SIZE)
        panic("Invalid page: %p", page);

    struct bitset_entry entry = get_bitset(page);
    if(!entry.bitset)
        panic("Page not found: %p", page);
    return bitset_test(entry.bitset, entry.index);
}

void pmm_free_range(unsigned long page, size_t length)
{
    if(page % PAGE_SIZE)
        panic("Invalid page: %p", page);
    else if(length % PAGE_SIZE)
        panic("Invalid length to free: 0x%X", length);

    for(unsigned long p = page; p < page + length; p += PAGE_SIZE) {
        struct bitset_entry entry = get_bitset(p);
        if(!entry.bitset)
            panic("Page not found: %p", p);
        else if(!bitset_test(entry.bitset, entry.index))
            panic("Page already free: %p (index: %d)", p, entry.index);
        bitset_clear(entry.bitset, entry.index);
    }
}

void pmm_free(unsigned long page)
{
    pmm_free_range(page, PAGE_SIZE);
}

void test_pmm()
{
    trace(" -= Testing pmm =-");

    /* Dump */
    struct bitmap_node* node;
    TAILQ_FOREACH(node, &bitmaps, next) {
        trace("node: base: 0x%X, count: %d, bitcount: %d",
              node->base,
              node->bitset->count,
              node->bitset->bitcount);
    }

    /* reserve first page */
    pmm_reserve(0);
    //pmm_reserve(1);   /* panic: invalid page */
    //pmm_reserve(0);   /* panic: page already reserved */

    /* reserve a range of pages from first one to 640k */
    // 0x9FC00
    pmm_reserve_range(0x1000, 0x9F000 - 0x1000);
    for(size_t page = 0; page < 0x9F000; page += 0x1000) {
        assert(pmm_exists(page));
        assert(pmm_reserved(page));
    }
    //pmm_free_range(0, 0x9F000 - 0x1000);

    /* Free a page */
    pmm_free(0x5000);
    for(size_t page = 0; page < 0x9F000; page += 0x1000) {
        if(page == 0x5000)
            assert(!pmm_reserved(page));
        else
            assert(pmm_reserved(page));
    }

    /* Free last page */
    //pmm_free(0x9F000);      /* page not found (last page is 0x9E000) */
    assert(!pmm_exists(0x9F000));
    pmm_free(0x9E000);

    /* Reserve a page from upper memory */
    pmm_reserve(0x100000);
    for(size_t page = 0; page < 0x9F000; page += 0x1000) {
        if(page == 0x5000)
            assert(!pmm_reserved(page));
        else if(page == 0x9E000)
            assert(!pmm_reserved(page));
        else
            assert(pmm_reserved(page));
    }
    for(size_t page = 0x100000; page < 0x7E0000; page += 0x1000) {
        if(page == 0x100000)
            assert(pmm_reserved(page));
        else
            assert(!pmm_reserved(page));
    }

    /* free previsously reserved range */
    pmm_reserve(0x5000);
    pmm_reserve(0x9E000);
    pmm_free_range(0, 0x9F000);

    trace(" -= Done testing pmm =-");
}

