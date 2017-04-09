#include "kmalloc.h"
#include "kernel.h"
#include "pmm.h"
#include "util.h"
#include "heap.h"
#include "debug.h"
#include "registers.h"
#include "locks.h"
#include "string.h"
#include "multiboot.h"
#include "vmm.h"

struct heap* kernel_heap = NULL;
static bool trace_enabled = false;

void kmalloc_init()
{
    /* 
     * We place an initial kernel heap just after the multiboot heap
     */
    struct heap_info mi_heap = multiboot_heap_info();
    unsigned char* heap_start = (unsigned char*)ALIGN((uint32_t)mi_heap.address + mi_heap.size,
                                                      PAGE_SIZE);
    unsigned max_size = (unsigned char*)KERNEL_LO_END - heap_start;
    kernel_heap = heap_init(heap_start, PAGE_SIZE * 64, max_size);
}

void* kmalloc(unsigned size)
{
    assert(size);

    void* result = kmalloc_a(size, sizeof(uint64_t));

    return result;
}

void kfree(void* address)
{
    enter_critical_section();

    if(address) {
        struct heap_block_header* block =
            (struct heap_block_header*)((unsigned char*)address - sizeof(struct heap_block_header));

        if(trace_enabled) {
            trace("kfree(%p); /* block: %p, flags: %p, size: %d, next: %p */", 
                  address,
                  block,
                  block->flags,
                  block->size,
                  block->next);
        }

        heap_free_block(kernel_heap, block);
    }


    leave_critical_section();
}

void* kmalloc_a(unsigned size, unsigned alignment)
{
    assert(size);

    enter_critical_section();

    unsigned char* result = NULL;
    struct heap_block_header* block = heap_alloc_block_aligned(kernel_heap,
                                                               size,
                                                               alignment);
    if(!block)
        panic("Kernel heap exhausted");

    if(block) {
        result = ((unsigned char*)block) + sizeof(struct heap_block_header);

        if(trace_enabled) {
            trace("kmalloc_a(%d, %d); /* block: %p, buffer: %p, flags: %p, size: %d, next: %p */",
                  size, alignment,
                  block,
                  result,
                  block->flags,
                  block->size,
                  block->next);
        }
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






