#include "kmalloc.h"
#include "debug.h"

extern unsigned char _kernel_end;      /* Put here by linker */
static bool paging_enabled = false;
static unsigned char* _kheap_start = &_kernel_end;

void* kmalloc(uint32_t size) {
    return kmalloc_ap(size, 1, nullptr);
}

void kfree(void* ptr) {
    assert(!paging_enabled);
}

/*
    Allocates memory aligned to the specified size, and returns physical location
*/
void* kmalloc_ap(uint32_t size, unsigned alignment, uint32_t* physical) {
    assert(!paging_enabled);

    unsigned char* ret = align(_kheap_start, alignment ? alignment : 1) + size;
    _kheap_start = ret + 1;
    if(physical)
        *physical = (uint32_t)ret;
    return ret;
}

uint32_t kheap_start() {
    return (uint32_t)_kheap_start;
}
