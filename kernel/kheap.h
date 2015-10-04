#ifndef _KHEAP_H_
#define _KHEAP_H_

#include <stdint.h>
#include "heap.h"

class KHeap {
private:
    static Heap _kheap;
    
    static void* alloc_impl(unsigned size, unsigned alignment, uint32_t* physical);
public:
    static void init();
    static void dump();

    template<typename T>
    static T* alloc(unsigned size, unsigned alignment = 1, uint32_t* physical = nullptr) { /* alloc memory */
        return (T*)alloc_impl(size, alignment, physical);
    }

    static void free(void* ptr);                /* free allocated pointer */

    static uint8_t* start();                    /* start of kernel heap */
    static uint8_t* end();                      /* end of kernel heap */

    static void check();                    /* check integrity */

    static void test();
};

#endif //_KHEAP_H_
