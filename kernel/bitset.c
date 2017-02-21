#include "bitset.h"
#include "kmalloc.h"
#include "string.h"
#include "debug.h"
#include "util.h"

#define BIT_PER_ELEMENT 32

struct bitset* bitset_new(uint32_t size)
{
    uint32_t element_count = ALIGN(size, BIT_PER_ELEMENT) / BIT_PER_ELEMENT;
    uint32_t alloc_size = sizeof(struct bitset) + (sizeof(uint32_t) * element_count);

    assert(element_count * BIT_PER_ELEMENT >= size);

    struct bitset* result = kmalloc(alloc_size);
    result->size = size;
    result->element_count = element_count;
    bitset_clear_all(result);

    return result;
}

bool bitset_test(const struct bitset* bitset, uint32_t offset)
{
    assert(offset < bitset->size);

    uint32_t element = offset / BIT_PER_ELEMENT;
    uint32_t mask = 1 << (offset % BIT_PER_ELEMENT);

    return bitset->data[element] & mask;
}

void bitset_set(struct bitset* bitset, uint32_t offset)
{
    assert(offset < bitset->size);

    uint32_t element = offset / BIT_PER_ELEMENT;
    uint32_t mask = 1 << (offset % BIT_PER_ELEMENT);
    
    bitset->data[element] |= mask;
}

void bitset_set_all(struct bitset* bitset)
{
    memset(bitset->data, 0xFFFFFFFF, bitset->element_count * sizeof(bitset->data[0]));
}

void bitset_clear(struct bitset* bitset, uint32_t offset)
{
    assert(offset < bitset->size);

    uint32_t element = offset / BIT_PER_ELEMENT;
    uint32_t mask = ~(1 << (offset % BIT_PER_ELEMENT));
    
    bitset->data[element] &= mask;
}

void bitset_clear_all(struct bitset* bitset)
{
    memset(bitset->data, 0, bitset->element_count * sizeof(bitset->data[0]));
}


