#include "kmalloc.h"
#include "kernel.h"
#include "pmm.h"
#include "util.h"
#include "heap.h"
#include "debug.h"
#include "registers.h"
#include "locks.h"

struct heap* kernel_heap = NULL;

void kmalloc_init()
{
    /* 
     * We place an initial kernel heap at KERNEL_END - (4Mb - 1)
     */
    unsigned char* heap_start = (unsigned char*)ALIGN(KERNEL_END, PAGE_SIZE);
    unsigned heap_limit = (4 * 1024 * 1024) - (unsigned)heap_start - 1;
    kernel_heap = heap_init(heap_start, PAGE_SIZE * 64, heap_limit);
}

void* kmalloc(unsigned size)
{
    assert(size);

    void* result = kmalloc_a(size, sizeof(int));
    return result;
}

void kfree(void* address)
{
    enter_critical_section();

    if(address) {
        struct heap_block_header* block =
            (struct heap_block_header*)((unsigned char*)address - sizeof(struct heap_block_header));
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
    assert(block);
    if(block) {
        result = ((unsigned char*)block) + sizeof(struct heap_block_header);
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


