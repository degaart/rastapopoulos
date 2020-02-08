#include "kernel.h"
#include "kmalloc.h"
#include "util.h"
#include "debug.h"
#include "string.h"
#include "pmm.h"
#include "vmm.h"

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

void* kmalloc_aligned(size_t size, size_t alignment)
{
    void* result = dlmemalign(size, alignment);
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
        assert((((unsigned long)result) % PAGE_SIZE) == 0);
        size = ALIGN(size, PAGE_SIZE);
        if(vmm_initialized()) {
            size_t remaining = size;

            unsigned char* page = heap_ptr;
            while(remaining) {
                unsigned long frame = pmm_find(size);
                pmm_reserve(frame);
                vmm_map(page, frame, VMM_PAGE_WRITABLE);

                remaining -= PAGE_SIZE;
                page += PAGE_SIZE;
            }
        } else if(pmm_initialized()) {
            pmm_reserve_range((unsigned long)heap_ptr, size);
        }
        heap_ptr += size;
        return result;
    } else {
        assert((size % PAGE_SIZE) == 0);
        if(vmm_initialized()) {
            signed long remaining = -size;
            unsigned char* start = heap_ptr - remaining;
            unsigned char* end = start + remaining;
            //trace("start: %p, end: %p, remaining: %p", start, end, remaining);
            for(unsigned char* page = start; page < end; page += PAGE_SIZE) {
                uint32_t frame = vmm_get_frame(page);
                pmm_free(frame);
                vmm_unmap(page);
            }
        } else if(pmm_initialized()) {
            panic("Not implemented yet");
        }
        heap_ptr += size;
        return heap_ptr;
    }
    return NULL;
}

void* kmalloc_brk()
{
    return heap_ptr;
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


