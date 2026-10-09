#include "kmalloc.h"
#include "early_malloc.h"
#include "heap.h"
#include "kernel.h"
#include "pmm.h"
#include "vmm.h"
#include <stdio.h>

#define HEAP_GROW_LIMIT 0x4000000

static void* heap_tail;

bool heap_init(void)
{
    heap_tail = early_malloc_tail();
    heap_tail = (void*)ALIGN_UP((uintptr_t)heap_tail, VMM_PAGE_SIZE);
    return true;
}

void* kmalloc(size_t size)
{
    return heap_alloc(size);
}

void kfree(void* ptr)
{
    return heap_free(ptr);
}

void* kmemalign(size_t alignment, size_t size)
{
    return heap_alloc_aligned(size, alignment);
}

static void cleanup_tail(void* start, size_t len)
{
    while (len) {
        if (!vmm_unmap(start))
            panic("Logic error");
        len -= VMM_PAGE_SIZE;
        start += VMM_PAGE_SIZE;
    }
}

void* heap_grow(size_t bytes)
{
    if (bytes & (VMM_PAGE_SIZE - 1))
        return NULL;

    void* initial_tail = heap_tail;
    while (bytes) {
        void* frame = pmm_alloc();
        if (!frame) {
            // printf("Out of memory while growing heap %zu bytes\n", bytes);
            cleanup_tail(initial_tail, heap_tail - initial_tail);
            return NULL;
        }

        if (!vmm_map(frame, heap_tail, VMM_WRITABLE)) {
            // printf("Out of memory while growing heap %zu bytes\n", bytes);
            cleanup_tail(initial_tail, heap_tail - initial_tail);
            return NULL;
        }

        heap_tail += VMM_PAGE_SIZE;
        bytes -= VMM_PAGE_SIZE;
    }
    return initial_tail;
}

void heap_shrink(size_t bytes)
{
    if (bytes & (VMM_PAGE_SIZE - 1))
        panic("Logic error");

    void* new_tail = heap_tail - bytes;
    for (void* ptr = new_tail; ptr < heap_tail; ptr += VMM_PAGE_SIZE) {
        void* frame;
        if (!vmm_frame(ptr, &frame))
            panic("Failed to get frame for address 0x%p", ptr);

        if (!vmm_unmap(ptr))
            panic("Logic error");

        pmm_free(frame);
    }

    heap_tail = new_tail;
}

