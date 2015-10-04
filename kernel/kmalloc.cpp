#include "kmalloc.h"
#include "kheap.h"

EXPORT
void* kmalloc_ap(uint32_t size, unsigned alignment, uint32_t* physical) {
    return KHeap::alloc<void>(size, alignment, physical);
}

EXPORT
void* kmalloc(uint32_t size) {
    return kmalloc_ap(size, 1, nullptr);
}

EXPORT
void kfree(void* ptr) {
    KHeap::free(ptr);
}
