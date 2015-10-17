#include "kheap.h"
#include "util.h"
#include "vmm.h"
#include "pmm.h"
#include "kmalloc.h"
#include "string.h"

Heap KHeap::_kheap;
static const int INITIAL_HEAP_SIZE = VMM::PAGE_SIZE;

#ifdef __APPLE__
unsigned char _KERNEL_END_[INITIAL_HEAP_SIZE * 128];
#endif

void KHeap::init() {
    if(VMM::paging_enabled())
        PANIC("KHeap must be initialized prior to enabling paging");

    _kheap.init(align(_KERNEL_END_, VMM::PAGE_SIZE), INITIAL_HEAP_SIZE);
}

void KHeap::dump() {
    _kheap.dump();
}

void* KHeap::alloc_impl(unsigned size, unsigned alignment) {
    check();

    if(!VMM::paging_enabled()) {
        void* ptr = _kheap.alloc<void>(size, alignment);
        while(!ptr) {
            /* Not enough space, must grow heap */
            unsigned grow_size = align(size, VMM::PAGE_SIZE);
            TRACE("Growing kernel heap size by %d bytes", grow_size);
            _kheap.grow(grow_size);
            
#ifdef __APPLE__
            assert(end() < _KERNEL_END_ + (INITIAL_HEAP_SIZE * 128));
#endif
            
            
            ptr = _kheap.alloc<void>(size, alignment);
        }

        check();
        return ptr;
    } else {
        void* ptr = _kheap.alloc<void>(size, alignment);
        while(!ptr) {
            unsigned grow_size = align(size, VMM::PAGE_SIZE);
            unsigned grow_pages = grow_size / VMM::PAGE_SIZE;

            // TRACE("Growing kernel heap by %d pages (end: %p) ", grow_pages, end());

            /* Map new pages in */
            assert( reinterpret_cast<uint32_t>(_kheap.limit()) % VMM::PAGE_SIZE == 0 );
            for(unsigned i = 0; i<grow_pages; i++) {
                if(end() + grow_size >= (uint8_t*)VMM::USERSPACE_START) {
                    return nullptr;
                }
                uint32_t page = PMM::alloc();
                VMM::map( end() + (i * VMM::PAGE_SIZE), page, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE);
            }

            _kheap.grow(grow_size);
            ptr = _kheap.alloc<void>(size, alignment);
        }

        return ptr;
    }
}

void KHeap::free(void* ptr) {
    check();
    
    if(ptr)
        _kheap.free(ptr);

    check();
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

    // uint32_t physical = 0xFFFFFFFF;
    uint8_t* p3 = alloc<uint8_t>(VMM::PAGE_SIZE, 4096);
    dump();
    // TRACE("p3: %p, phys 0x%X", p3, physical);

    uint8_t* p4[64];
    Util::srand(0xDEADBEEF);    
    for(unsigned i = 0; i < sizeof(p4) / sizeof(*p4); i++) {
        bool align = !(Util::rand() % 2);
        unsigned size = (Util::rand() % ((VMM::PAGE_SIZE) * 2));
        if(size) {
            TRACE("Allocating %d bytes %s", size, align ? "aligned" : "");

            uint8_t* p = alloc<uint8_t>(size, align ? VMM::PAGE_SIZE : 1);
            TRACE("p: %p", p);
            dump();
            bzero(p, size);


            p4[i] = p;
        } else {
            p4[i] = nullptr;
        }
    }

    for(unsigned i = 0; i < sizeof(p4) / sizeof(*p4); i++)
        free(p4[i]);

    free(p3);
    free(p2);
    free(p1);
    free(p0);
    dump();
}

void KHeap::check() {
    _kheap.check();
}

