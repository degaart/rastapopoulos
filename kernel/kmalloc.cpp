#include "kmalloc.h"
#include "debug.h"
#include "vmm.h"

static unsigned char* _kheap_start = _KERNEL_END_;

void* kmalloc(uint32_t size) {
    return kmalloc_ap(size, 1, nullptr);
}

void kfree(void* ptr) {
    assert(!VMM::paging_enabled());
}

/*
    Allocates memory aligned to the specified alignment, and returns physical location
*/
void* kmalloc_ap(uint32_t size, unsigned alignment, uint32_t* physical) {
    assert(!VMM::paging_enabled());

    unsigned char* ret = align(_kheap_start, alignment ? alignment : 1);
    _kheap_start = align(_kheap_start + size, 2);
    if(physical)
        *physical = (uint32_t)ret;
    return ret;
}

uint32_t kheap_start() {
    return (uint32_t)_kheap_start;
}
