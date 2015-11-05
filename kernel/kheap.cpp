#include "kheap.h"
#include "util.h"
#include "vmm.h"
#include "pmm.h"
#include "kmalloc.h"
#include "string.h"

Heap* KHeap::_kheap;
static const int INITIAL_HEAP_SIZE = VMM::PAGE_SIZE;

#ifdef __APPLE__
unsigned char _KERNEL_END_[INITIAL_HEAP_SIZE * 128];
#endif

void KHeap::init() {
    if(VMM::paging_enabled())
        PANIC("KHeap must be initialized prior to enabling paging");

    /* Manually initialize KHeap without calling malloc */
    uint8_t* kernel_end = _KERNEL_END_;
    _kheap = (Heap*)kernel_end;
    kernel_end += sizeof(Heap);

    _kheap->init(align(kernel_end, VMM::PAGE_SIZE), INITIAL_HEAP_SIZE);
}

void KHeap::dump() {
    _kheap->dump();
}

void* KHeap::alloc_impl(unsigned size, unsigned alignment) {
    check();

    if(!VMM::paging_enabled()) {
        void* ptr = _kheap->alloc<void>(size, alignment);
        while(!ptr) {
            /* Not enough space, must grow heap */
            unsigned grow_size = align(size, VMM::PAGE_SIZE);
            TRACE("Growing kernel heap size by %d bytes", grow_size);
            _kheap->grow(grow_size);
            
#ifdef __APPLE__
            assert(end() < _KERNEL_END_ + (INITIAL_HEAP_SIZE * 128));
#endif
            
            
            ptr = _kheap->alloc<void>(size, alignment);
        }

        check();
        return ptr;
    } else {
        void* ptr = _kheap->alloc<void>(size, alignment);
        while(!ptr) {
            unsigned grow_size = align(size, VMM::PAGE_SIZE);
            unsigned grow_pages = grow_size / VMM::PAGE_SIZE;

            // TRACE("Growing kernel heap by %d pages (end: %p) ", grow_pages, end());

            /* Map new pages in */
            assert( reinterpret_cast<uint32_t>(_kheap->limit()) % VMM::PAGE_SIZE == 0 );
            for(unsigned i = 0; i<grow_pages; i++) {
                if(end() + grow_size >= (uint8_t*)VMM::USERSPACE_START) {
                    return nullptr;
                }
                uint32_t page = PMM::alloc();
                VMM::map( end() + (i * VMM::PAGE_SIZE), page, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE);
            }

            _kheap->grow(grow_size);
            ptr = _kheap->alloc<void>(size, alignment);
        }

        return ptr;
    }
}

void KHeap::free(void* ptr) {
    check();
    
    if(ptr)
        _kheap->free(ptr);

    check();
}

uint8_t* KHeap::start() {
    return _kheap->head();
}

uint8_t* KHeap::end() {
    return _kheap->head() + _kheap->total_size();
}

void KHeap::check() {
    _kheap->check();
}

