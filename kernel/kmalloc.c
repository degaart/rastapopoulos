#include "kmalloc.h"
#include "early_malloc.h"
#include "heap.h"
#include "kernel.h"
#include "pmm.h"
#include "trace.h"
#include "vmm.h"
#include <stdio.h>

#define HEAP_GROW_LIMIT 0x4000000

static void* heap_tail;

bool heap_init(void)
{
    heap_tail = early_malloc_tail();
    heap_tail = (void*)ALIGN_UP((uintptr_t)heap_tail, PAGE_SIZE);
    return true;
}

void* kmalloc(size_t size)
{
    bool flag = enter_critical_section();
    void* result = heap_alloc(size);
    leave_critical_section(flag);
    return result;
}

void kfree(void* ptr)
{
    bool flag = enter_critical_section();
    heap_free(ptr);
    leave_critical_section(flag);
}

void* kmemalign(size_t alignment, size_t size)
{
    bool flag = enter_critical_section();
    void* result = heap_alloc_aligned(size, alignment);
    leave_critical_section(flag);
    return result;
}

static void cleanup_tail(void* start, size_t len)
{
    void* rollback_start = start;
    trace("heap rollback: start=%p pages=%zu free_before=%zu", start,
          len / PAGE_SIZE, pmm_info());
    while (len) {
        void* frame;
        if (!vmm_frame(start, &frame))
            panic("Failed to get frame for address 0x%p", start);
        if (!vmm_unmap(start))
            panic("Logic error");
        len -= PAGE_SIZE;
        start += PAGE_SIZE;
    }
    heap_tail = rollback_start;
    trace("heap rollback complete: free_after=%zu", pmm_info());
}

void* heap_grow(size_t bytes)
{
    if (bytes & (PAGE_SIZE - 1))
        return NULL;

    void* initial_tail = heap_tail;
    while (bytes) {
        void* frame = pmm_alloc();
        if (!frame) {
            trace("heap_grow: physical allocation failed, mapped_pages=%zu",
                  (heap_tail - initial_tail) / PAGE_SIZE);
            cleanup_tail(initial_tail, heap_tail - initial_tail);
            return NULL;
        }

        if (!vmm_map(frame, heap_tail, VMM_WRITABLE)) {
            trace("heap_grow: mapping failed at %p, mapped_pages=%zu",
                  heap_tail, (heap_tail - initial_tail) / PAGE_SIZE);
            pmm_free(frame);
            cleanup_tail(initial_tail, heap_tail - initial_tail);
            return NULL;
        }

        heap_tail += PAGE_SIZE;
        bytes -= PAGE_SIZE;
    }
    return initial_tail;
}

void heap_shrink(size_t bytes)
{
    if (bytes & (PAGE_SIZE - 1))
        panic("Logic error");

    void* new_tail = heap_tail - bytes;
    for (void* ptr = new_tail; ptr < heap_tail; ptr += PAGE_SIZE) {
        void* frame;
        if (!vmm_frame(ptr, &frame))
            panic("Failed to get frame for address 0x%p", ptr);

        if (!vmm_unmap(ptr))
            panic("Logic error");
    }

    heap_tail = new_tail;
}

