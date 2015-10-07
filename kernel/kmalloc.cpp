#include "kmalloc.h"
#include "kheap.h"

EXPORT
void* kmalloc_a(uint32_t size, unsigned alignment) {
    return KHeap::alloc<void>(size, alignment);
}

EXPORT
void* kmalloc(uint32_t size) {
    return kmalloc_a(size, 1);
}

EXPORT
void kfree(void* ptr) {
    KHeap::free(ptr);
}
