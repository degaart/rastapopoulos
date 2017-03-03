#include "kmalloc.h"
#include "kernel.h"
#include "pmm.h"
#include "util.h"
#include "heap.h"
#include "debug.h"

struct heap* kernel_heap = NULL;

void kmalloc_init()
{
    /* 
     * We place an initial kernel heap at KERNEL_END - (4Mb - 1)
     */
    unsigned char* heap_start = (unsigned char*)ALIGN(KERNEL_END, PAGE_SIZE);
    unsigned heap_limit = (4 * 1024 * 1024) - (unsigned)heap_start - 1;
    kernel_heap = heap_init(heap_start, PAGE_SIZE, heap_limit);
}

void* kmalloc(unsigned size)
{
    void* result = kmalloc_a(size, sizeof(int));
    return result;
}

void kfree(void* address)
{
    if(address) {
        struct heap_block_header* block =
            (struct heap_block_header*)((unsigned char*)address - sizeof(struct heap_block_header));
        heap_free_block(kernel_heap, block);
    }
}

void* kmalloc_a(unsigned size, unsigned alignment)
{
    void* result = NULL;
    struct heap_block_header* block = heap_alloc_block_aligned(kernel_heap,
                                                               size,
                                                               alignment);
    assert(block);
    if(block) {
        result = ((unsigned char*)block) + sizeof(struct heap_block_header);
    }
    return result;
}

void kernel_heap_info(struct kernel_heap_info* buffer)
{
    struct heap_info hi = heap_info(kernel_heap);
    buffer->heap_start = (uint32_t)hi.address;
    buffer->heap_size = hi.size;
}


