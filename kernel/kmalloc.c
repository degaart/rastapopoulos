#include "kernel.h"
#include "kmalloc.h"
#include "util.h"
#include "debug.h"
#include "string.h"

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
    void* result = dlmalloc(size);
    memset(result, 0xCC, size);
    return result;
}

void kfree(void* ptr)
{
    size_t* info_ptr = (size_t*)ptr;
    size_t size = (info_ptr[-1] & ~(1|2)) - 8;
    memset(info_ptr, 0xDD, size);
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

void test_kmalloc()
{
    trace(" -= Testing kmalloc =-");

    unsigned long* p0 = kmalloc(4096);
    unsigned long* p1 = kmalloc(8192);
    unsigned long* p2 = kmalloc(1);

    kfree(p2);
    kfree(p1);
    kfree(p0);

    trace(" -= Done testing kmalloc =-");
}


