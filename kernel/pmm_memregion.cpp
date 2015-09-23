#include "pmm.h"


PMM::MemRegion::MemRegion(uint32_t base, uint32_t size)
: _base(align(base, PAGE_SIZE)), _size(truncate(size, PAGE_SIZE)), _bitset(size / PAGE_SIZE), _free_size(_size / PAGE_SIZE) {

}

PMM::MemRegion::MemRegion(const MemRegion& region)
: _base{region._base}, _size{region._size}, _bitset{region._bitset}, _free_size{region._free_size} {

}


uint32_t PMM::MemRegion::base() const {
    return _base;
}

uint32_t PMM::MemRegion::size() const {
    return _size;
}

uint32_t PMM::MemRegion::pages() const {
    return size() / PAGE_SIZE;
}

uint32_t PMM::MemRegion::pages_free() const {
    return _free_size;
}

/* Checks if given page is inside region */
bool PMM::MemRegion::contains_page(uint32_t page) const {
    return (page >= base()) && (page <= base() + size());
}

/* Checks if given page is reserved, throws error if page already reserved, or base outside region */
bool PMM::MemRegion::page_reserved(uint32_t page) const {
    assert(contains_page(page));
    int idx = indexof(page);
    return _bitset.test(idx);
}

/* Reserves given page, throws error if page already reserved, or page outside region */
void PMM::MemRegion::reserve(uint32_t page) {
    assert(contains_page(page));
    int idx = indexof(page);
    assert(!_bitset.test(idx));
    _bitset.set(idx);
    _free_size--;
}

/* Frees given page, throws error if page already free, or page outside region */
void PMM::MemRegion::free(uint32_t page) {
    assert(contains_page(page));
    int idx = indexof(page);
    assert(_bitset.test(idx));
    _bitset.clear(idx);
    _free_size++;
}

unsigned PMM::MemRegion::indexof(uint32_t page) const {
    assert(contains_page(page));
    return (page - base()) / PAGE_SIZE;
}

bool PMM::MemRegion::find(uint32_t* page) const {
    unsigned index = _bitset.find();
    if(index == UINT_MAX)
        return false;
    *page = base() + (index * PAGE_SIZE);
    return true;
}

bool PMM::MemRegion::find(uint32_t* page, unsigned size) const {
    unsigned index = _bitset.find_range(align(size, PAGE_SIZE) / PAGE_SIZE);
    if(index == UINT_MAX)
        return false;
    *page = base() + (index * PAGE_SIZE);
    return true;
}

