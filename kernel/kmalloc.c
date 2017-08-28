#include "kmalloc.h"
#include "kernel.h"
#include "pmm.h"
#include "util.h"
#include "heap.h"
#include "kdebug.h"
#include "registers.h"
#include "locks.h"
#include "string.h"
#include "multiboot.h"
#include "vmm.h"

struct heap* kernel_heap = NULL;
static bool trace_enabled = false;

void kmalloc_init(void* start)
{
    unsigned char* heap_start = (unsigned char*)ALIGN((uint32_t)start, PAGE_SIZE);
    unsigned max_size = 0xC0400000 - (uint32_t)heap_start;
    kernel_heap = heap_init(heap_start, PAGE_SIZE * 64, max_size);
}

void* kmalloc(unsigned size)
{
    assert(size);
    unsigned char* result = kmalloc_a(size, sizeof(uint64_t));
    return result;
}

void kfree(void* address)
{
    enter_critical_section();

    if(address) {
        if(trace_enabled) {
            trace("kfree(%p)", address);
        }
        heap_free(kernel_heap, address);
    }

    leave_critical_section();
}

void* kmalloc_a(unsigned size, unsigned alignment)
{
    assert(size);

    enter_critical_section();

    unsigned char* result = heap_alloc_aligned(kernel_heap,
                                               size,
                                               alignment);
    if(!result) {
        panic("Kernel heap exhausted");
    }
    if(trace_enabled) {
        trace("kmalloc_a(%d, %d); /* %p */",
              size, alignment,
              result);
    }

    leave_critical_section();

    return result;
}

void kernel_heap_info(struct kernel_heap_info* buffer)
{
    enter_critical_section();

    struct heap_info hi = heap_info(kernel_heap);
    buffer->heap_start = (uint32_t)hi.address;
    buffer->heap_size = hi.size;

    leave_critical_section();
}

void heap_trace_start()
{
    trace_enabled = true;
}

void heap_trace_stop()
{
    trace_enabled = false;
}

void __kernel_heap_check(const char* file, int line)
{
    heap_check(kernel_heap, file, line);
}






