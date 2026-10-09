#include "heap.h"
#include <stdint.h>

#define HEAP_PAGE_SIZE 4096
#define HEAP_ALIGNMENT 16
#define HEAP_WORD_SIZE sizeof(void*)

struct HeapBlock
{
    size_t size; /* Payload bytes, excluding this header */
    struct HeapBlock* prev;
    struct HeapBlock* next;
    unsigned free;
};

static struct HeapBlock* heap_first;
static struct HeapBlock* heap_last;

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

    void* memory = heap_grow(bytes);
    if (!memory) {
        return NULL;
    }

    if (heap_last && heap_last->free) {
        heap_last->size += bytes;
        return heap_last;
    }

    struct HeapBlock* block = memory;
    block->size = bytes - sizeof(*block);
    block->prev = heap_last;
    block->next = NULL;
    block->free = 1;

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
        heap_shrink(bytes);
        return;
    }

    size_t bytes = block->size & ~(HEAP_PAGE_SIZE - 1);
    if (bytes) {
        block->size -= bytes;
        heap_shrink(bytes);
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

    size_t overhead = sizeof(struct HeapBlock*) + alignment - 1;
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

    struct HeapBlock* block = ((struct HeapBlock**)ptr)[-1];
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

    for (const struct HeapBlock* block = heap_first; block;
         block = block->next) {
        info.total_size += sizeof(*block) + block->size;
        info.overhead += sizeof(*block);

        if (block->free) {
            info.free_size += block->size;
        } else {
            info.overhead += sizeof(struct HeapBlock*);
            info.allocated_size += block->size - sizeof(struct HeapBlock*);
        }
    }

    return info;
}

