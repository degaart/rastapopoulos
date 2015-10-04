#include "kmalloc.h"
#include "debug.h"
#include "vmm.h"
#include "pmm.h"
#include "heap.h"

static unsigned char* _kheap_start = _KERNEL_END_;
static const int MAGIC = 'KMLC';

static void* kmalloc_ap_seg(uint32_t size, unsigned alignment, uint32_t* physical);

struct kmalloc_header_t {
    uint32_t magic;
    uint32_t heap_frame;
    unsigned size;
};

void* kmalloc_ap(uint32_t size, unsigned alignment, uint32_t* physical) {
    assert(!VMM::paging_enabled());
    return kmalloc_ap_seg(size, alignment, physical);
}

void kfree(void* ptr) {
    assert(!VMM::paging_enabled());
    return;
}

void* kmalloc(uint32_t size) {
    return kmalloc_ap(size, 1, nullptr);
}

/*
    Allocates memory aligned to the specified alignment, and returns physical location
*/
static void* kmalloc_ap_seg(uint32_t size, unsigned alignment, uint32_t* physical) {
    assert(!VMM::paging_enabled());

    unsigned char* ret = align(_kheap_start, alignment ? alignment : 1);
    _kheap_start = align(_kheap_start + size, 2);
    if(physical)
        *physical = (uint32_t)ret;
    return ret;
}


void* kmalloc_ap(uint32_t size, uint32_t* physical) {
    return kmalloc_ap(size, VMM::PAGE_SIZE, physical);
}

uint32_t kheap_start() {
    //assert(!VMM::paging_enabled());
    
    return (uint32_t)_kheap_start;
}
