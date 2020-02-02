#include "kernel.h"
#include "kmalloc.h"
#include "util.h"
#include "debug.h"

extern void* dlmalloc(size_t);
extern void dlfree(void*);
extern void* dlrealloc(void*, size_t);
extern void* dlmemalign(size_t, size_t);

static unsigned char* heap_ptr;

void kmalloc_init(void* kernel_end)
{
    heap_ptr = ALIGN(kernel_end, 4096);
}

void* kmalloc(size_t size)
{
    /* TODO: Fill memory with 0xCC for easier spotting in memory dumps */
    return dlmalloc(size);
}

void kfree(void* ptr)
{
    /* TODO: Fill memory with 0xDD for easier spotting in memory dumps */
    dlfree(ptr);
}

void* sbrk(ptrdiff_t size)
{
    if(size == 0) {
        return heap_ptr;
    } else if(size > 0) {
        void* result = heap_ptr;
        heap_ptr += ALIGN(size, 4096);
        return result;
    } else {
        heap_ptr += size;
        return heap_ptr;
    }
    return NULL;
}



