#include "kmalloc.h"
#include "kernel.h"
#include "pmm.h"
#include "util.h"

unsigned char* initial_kernel_heap = &_KERNEL_END_;

void* kmalloc(unsigned size)
{
    void* result = initial_kernel_heap;

    if(pmm_initialized() && !pmm_reserved(TRUNCATE((uint32_t)result, PAGE_SIZE)))
        pmm_reserve(TRUNCATE((uint32_t)result, PAGE_SIZE));

    initial_kernel_heap += size;
    return result;
}

void kernel_heap_info(struct kernel_heap_info* heap_info)
{
    heap_info->heap_start = KERNEL_END;
    heap_info->heap_size = (uint32_t)initial_kernel_heap - KERNEL_END;
}


