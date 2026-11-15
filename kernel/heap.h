#pragma once

#include <stddef.h>

void* heap_grow(size_t bytes);
void heap_shrink(size_t bytes);
void* heap_alloc(size_t size);
void* heap_alloc_aligned(size_t size, size_t alignment);
void heap_free(void* ptr);

struct HeapInfo
{
    size_t total_size;
    size_t free_size;
    size_t allocated_size;
    size_t overhead;
};

struct HeapInfo heap_info(void);

#if defined(UNIT_TESTS) && defined(HEAP_DEBUG)
const char* heap_test_validate(void* ptr);
#endif

struct HeapBlock
{
    size_t size; /* Payload bytes, excluding this header */
    struct HeapBlock* prev;
    struct HeapBlock* next;
    unsigned free;
#ifdef HEAP_DEBUG
    void* returned;
    size_t requested;
#endif
};

