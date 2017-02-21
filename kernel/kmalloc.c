#include "kmalloc.h"

extern unsigned char _KERNEL_END_;   /* Set by linker */
unsigned char* initial_kernel_heap = &_KERNEL_END_;

void* kmalloc(unsigned size)
{
    void* result = initial_kernel_heap;
    initial_kernel_heap += size;
    return result;
}








