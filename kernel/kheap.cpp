#include "kheap.h"
#include "util.h"
#include "vmm.h"
#include "kmalloc.h"

Heap KHeap::_kheap;
static const int INITIAL_HEAP_SIZE = VMM::PAGE_SIZE;

#ifdef __APPLE__
unsigned char _KERNEL_END_[INITIAL_HEAP_SIZE * 4];
#endif

void KHeap::init() {
    if(VMM::paging_enabled())
        PANIC("KHeap must be initialized prior to enabling paging");

    _kheap.init(align(_KERNEL_END_, VMM::PAGE_SIZE), INITIAL_HEAP_SIZE);
}

void KHeap::dump() {
    _kheap.dump();
}

void* KHeap::alloc_impl(unsigned size, unsigned alignment, uint32_t* physical) {
    if(!VMM::paging_enabled()) {
        void* ptr = _kheap.alloc<void>(size, alignment);
        while(!ptr) {
            /* Not enough space, must grow heap */
            unsigned grow_size = align(size, VMM::PAGE_SIZE);
            TRACE("Growing kernel heap size by %d bytes", grow_size);
            _kheap.grow(grow_size);
            ptr = _kheap.alloc<void>(size, alignment);
        }

        if(physical)
            *physical = (uint32_t)ptr;
        return ptr;
    } else {
        PANIC("Not implemented yet");
        return nullptr;
    }
}

void KHeap::free(void* ptr) {
    if(ptr)
        _kheap.free(ptr);
}

uint8_t* KHeap::start() {
    return _kheap.head();
}

uint8_t* KHeap::end() {
    return _kheap.head() + _kheap.total_size();
}

void KHeap::test() {
    dump();

    uint8_t* p0 = alloc<uint8_t>(64);
    dump();
    TRACE("p0: %p", p0);

    uint8_t* p1 = alloc<uint8_t>(VMM::PAGE_SIZE);
    dump();
    TRACE("p1: %p", p1);

    uint8_t* p2 = alloc<uint8_t>(VMM::PAGE_SIZE, 4096);
    dump();
    TRACE("p2: %p", p2);

    uint32_t physical = 0xFFFFFFFF;
    uint8_t* p3 = alloc<uint8_t>(VMM::PAGE_SIZE, 4096, &physical);
    dump();
    TRACE("p3: %p, phys 0x%X", p3, physical);

    Util::srand(0xDEADBEEF);    
    for(unsigned i = 0; i < 10; i++) {
        bool align = !(Util::rand() % 2);
        unsigned size = (Util::rand() % ((VMM::PAGE_SIZE) * 2));
        if(size) {
            TRACE("Allocating %d bytes %s", size, align ? "aligned" : "");

            uint8_t* p = alloc<uint8_t>(size, align ? VMM::PAGE_SIZE : 1);
            TRACE("p: %p", p);
            dump();


        }
    }
}
