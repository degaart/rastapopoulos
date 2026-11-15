#include "heap.h"
#include "kernel.h"
#include <stdint.h>

#define HEAP_PAGE_SIZE 4096
#define HEAP_ALIGNMENT 16
#define HEAP_WORD_SIZE sizeof(void*)

struct HeapBlock* heap_first;
struct HeapBlock* heap_last;

#ifdef HEAP_DEBUG
uintptr_t heap_begin;
uintptr_t heap_end;
#    define HEAP_GUARD_SIZE 8

static const char* heap_validate(void* ptr, struct HeapBlock** match)
{
    struct HeapBlock* previous = NULL;
    struct HeapBlock* found = NULL;
    uintptr_t expected = heap_begin;
    for (struct HeapBlock* block = heap_first; block; block = block->next) {
        uintptr_t address = (uintptr_t)block;
        if (address != expected || address % HEAP_WORD_SIZE ||
            address > heap_end || heap_end - address < sizeof(*block))
            return "header bounds";
        uintptr_t payload = address + sizeof(*block);
        if (block->size > heap_end - payload || block->size % HEAP_WORD_SIZE ||
            block->prev != previous || block->free > 1)
            return "block metadata";
        expected = payload + block->size;
        if ((expected == heap_end && (block->next || block != heap_last)) ||
            (expected < heap_end && (uintptr_t)block->next != expected))
            return "block adjacency";
        if (!block->free) {
            uintptr_t returned = (uintptr_t)block->returned;
            if (returned < payload + sizeof(struct HeapBlock*) ||
                returned > expected || returned % HEAP_ALIGNMENT ||
                !block->requested || block->requested > expected - returned ||
                expected - returned - block->requested < HEAP_GUARD_SIZE)
                return "requested-size bounds";
            if (block->returned == ptr)
                found = block;
        } else if (block->returned || block->requested) {
            return "free block metadata";
        }
        previous = block;
    }
    if (expected != heap_end || previous != heap_last)
        return "heap bounds";
    if (!ptr)
        return NULL;
    if (!found)
        return "invalid or repeated free";
    uintptr_t returned = (uintptr_t)found->returned;
    if (((struct HeapBlock**)returned)[-1] != found)
        return "hidden block pointer";
    unsigned char* guard = (unsigned char*)returned + found->requested;
    for (size_t i = 0; i < HEAP_GUARD_SIZE; i++)
        if (guard[i] != 0xa7)
            return "trailing canary";
    if (match)
        *match = found;
    return NULL;
}

static void heap_check(void)
{
    const char* error = heap_validate(NULL, NULL);
    if (error)
        panic("Heap protection: %s, pointer=0x%p", error, NULL);
}

#    ifdef UNIT_TESTS
const char* heap_test_validate(void* ptr)
{
    return heap_validate(ptr, NULL);
}
#    endif
#endif

#ifdef UNIT_TESTS
extern unsigned char heap_test_memory[32768] __attribute__((aligned(4096)));
extern size_t heap_test_used;
extern unsigned heap_testing;
#endif

static void* heap_acquire(size_t bytes)
{
#ifdef UNIT_TESTS
    if (heap_testing) {
        if (bytes > sizeof(heap_test_memory) - heap_test_used)
            return NULL;
        void* result = heap_test_memory + heap_test_used;
        heap_test_used += bytes;
        return result;
    }
#endif
    return heap_grow(bytes);
}

static void heap_release(size_t bytes)
{
#ifdef HEAP_DEBUG
    heap_end -= bytes;
    if (!heap_first)
        heap_begin = heap_end = 0;
#endif
#ifdef UNIT_TESTS
    if (heap_testing) {
        heap_test_used -= bytes;
        return;
    }
#endif
    heap_shrink(bytes);
}

static int heap_round_up(size_t value, size_t alignment, size_t* result)
{
    if (value > SIZE_MAX - (alignment - 1)) {
        return 0;
    }

    *result = (value + alignment - 1) & ~(alignment - 1);
    return 1;
}

static void heap_merge_next(struct HeapBlock* block)
{
    struct HeapBlock* next = block->next;
    block->size += sizeof(*next) + next->size;
    block->next = next->next;

    if (block->next) {
        block->next->prev = block;
    } else {
        heap_last = block;
    }
}

static void heap_split(struct HeapBlock* block, size_t size)
{
    if (block->size - size < sizeof(*block) + HEAP_WORD_SIZE) {
        return;
    }

    struct HeapBlock* remainder =
        (struct HeapBlock*)((unsigned char*)(block + 1) + size);

    remainder->size = block->size - size - sizeof(*block);
    remainder->prev = block;
    remainder->next = block->next;
    remainder->free = 1;
#ifdef HEAP_DEBUG
    remainder->returned = NULL;
    remainder->requested = 0;
#endif

    if (remainder->next) {
        remainder->next->prev = remainder;
    } else {
        heap_last = remainder;
    }

    block->size = size;
    block->next = remainder;
}

static struct HeapBlock* heap_extend(size_t need)
{
    size_t bytes;

    if (heap_last && heap_last->free) {
        bytes = need - heap_last->size;
    } else {
        if (need > SIZE_MAX - sizeof(struct HeapBlock)) {
            return NULL;
        }

        bytes = need + sizeof(struct HeapBlock);
    }

    if (!heap_round_up(bytes, HEAP_PAGE_SIZE, &bytes)) {
        return NULL;
    }

    void* memory = heap_acquire(bytes);
    if (!memory) {
        return NULL;
    }

#ifdef HEAP_DEBUG
    if (!heap_first)
        heap_begin = heap_end = (uintptr_t)memory;
    if ((uintptr_t)memory != heap_end || bytes > UINTPTR_MAX - heap_end)
        panic("Heap protection: growth bounds, pointer=0x%p", memory);
    heap_end += bytes;
#endif
    if (heap_last && heap_last->free) {
        heap_last->size += bytes;
        return heap_last;
    }

    struct HeapBlock* block = memory;
    block->size = bytes - sizeof(*block);
    block->prev = heap_last;
    block->next = NULL;
    block->free = 1;
#ifdef HEAP_DEBUG
    block->returned = NULL;
    block->requested = 0;
#endif

    if (heap_last) {
        heap_last->next = block;
    } else {
        heap_first = block;
    }

    heap_last = block;
    return block;
}

static void heap_trim(void)
{
    struct HeapBlock* block = heap_last;
    if (!block || !block->free) {
        return;
    }

    if (block == heap_first) {
        size_t bytes = sizeof(*block) + block->size;
        heap_first = NULL;
        heap_last = NULL;
        heap_release(bytes);
        return;
    }

    size_t bytes = block->size & ~(HEAP_PAGE_SIZE - 1);
    if (bytes) {
        block->size -= bytes;
        heap_release(bytes);
    }
}

void* heap_alloc_aligned(size_t size, size_t alignment)
{
    if (!size || !alignment || (alignment & (alignment - 1))) {
        return NULL;
    }
    if (alignment < HEAP_ALIGNMENT) {
        alignment = HEAP_ALIGNMENT;
    }

#ifdef HEAP_DEBUG
    heap_check();
#endif
    size_t overhead = sizeof(struct HeapBlock*) + alignment - 1;
#ifdef HEAP_DEBUG
    if (overhead > SIZE_MAX - HEAP_GUARD_SIZE)
        return NULL;
    overhead += HEAP_GUARD_SIZE;
#endif
    if (size > SIZE_MAX - overhead) {
        return NULL;
    }

    size_t need;
    if (!heap_round_up(size + overhead, HEAP_WORD_SIZE, &need)) {
        return NULL;
    }

    struct HeapBlock* block = heap_first;
    for (; block; block = block->next) {
        if (block->free && block->size >= need) {
            break;
        }
    }

    if (!block) {
        block = heap_extend(need);
        if (!block) {
            return NULL;
        }
    }

    heap_split(block, need);
    block->free = 0;
    uintptr_t address = (uintptr_t)(block + 1) + sizeof(struct HeapBlock*);
    address = (address + alignment - 1) & ~(uintptr_t)(alignment - 1);
    ((struct HeapBlock**)address)[-1] = block;
#ifdef HEAP_DEBUG
    block->returned = (void*)address;
    block->requested = size;
    for (size_t i = 0; i < HEAP_GUARD_SIZE; i++)
        ((unsigned char*)address)[size + i] = 0xa7;
#endif
    return (void*)address;
}

void* heap_alloc(size_t size)
{
    return heap_alloc_aligned(size, HEAP_ALIGNMENT);
}

void heap_free(void* ptr)
{
    if (!ptr) {
        return;
    }
#ifdef HEAP_DEBUG
    struct HeapBlock* block = NULL;
    const char* error = heap_validate(ptr, &block);
    if (error)
        panic("Heap protection: %s, pointer=0x%p", error, ptr);
    block->returned = NULL;
    block->requested = 0;
#else
    if (
#    ifdef UNIT_TESTS
        !heap_testing &&
#    endif
        (uint8_t*)ptr >= __kernel_start && (uint8_t*)ptr < __kernel_end)
        panic("Attempt to free an invalid pointer: 0x%p", ptr);
    struct HeapBlock* block = ((struct HeapBlock**)ptr)[-1];
#endif
    block->free = 1;
    if (block->next && block->next->free) {
        heap_merge_next(block);
    }

    if (block->prev && block->prev->free) {
        block = block->prev;
        heap_merge_next(block);
    }

    heap_trim();
}

struct HeapInfo heap_info(void)
{
    struct HeapInfo info = {0};
#ifdef HEAP_DEBUG
    heap_check();
#endif

    for (const struct HeapBlock* block = heap_first; block;
         block = block->next) {
        info.total_size += sizeof(*block) + block->size;
        info.overhead += sizeof(*block);

        if (block->free) {
            info.free_size += block->size;
        } else {
#ifdef HEAP_DEBUG
            info.overhead += block->size - block->requested;
            info.allocated_size += block->requested;
#else
            info.overhead += sizeof(struct HeapBlock*);
            info.allocated_size += block->size - sizeof(struct HeapBlock*);
#endif
        }
    }

    return info;
}

