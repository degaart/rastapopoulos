#include "kmalloc.h"
#include "kernel.h"
#include "pmm.h"
#include "util.h"

uint32_t initial_kernel_heap = (uint32_t)&_KERNEL_END_;

void* kmalloc(unsigned size)
{
    void* result = kmalloc_a(size, 4);
    return result;
}

void* kmalloc_a(unsigned size, unsigned alignment)
{
    uint32_t result = initial_kernel_heap;
    result = ALIGN(result, alignment);

    if(pmm_initialized() && !pmm_reserved(TRUNCATE(result, PAGE_SIZE)))
        pmm_reserve(TRUNCATE(result, PAGE_SIZE));

    initial_kernel_heap = result + size;
    return (void*)result;
}

void kernel_heap_info(struct kernel_heap_info* heap_info)
{
    heap_info->heap_start = KERNEL_END;
    heap_info->heap_size = (uint32_t)initial_kernel_heap - KERNEL_END;
}


