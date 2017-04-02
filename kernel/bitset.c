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

unsigned bitset_alloc_size(uint32_t size)
{
    uint32_t element_count = ALIGN(size, BIT_PER_ELEMENT) / BIT_PER_ELEMENT;
    uint32_t alloc_size = sizeof(struct bitset) + (sizeof(uint32_t) * element_count);
    assert(element_count * BIT_PER_ELEMENT >= size);
    return alloc_size;
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

uint32_t bitset_find(struct bitset* bitset, unsigned value)
{
    if(value) {
        unsigned element_index = BITSET_INVALID_INDEX;
        for(unsigned i = 0; i < bitset->element_count; i++) {
            if(bitset->data[i]) {
                element_index = i;
                break;
            }
        }

        if(element_index == BITSET_INVALID_INDEX)
            return BITSET_INVALID_INDEX;

        uint32_t start_index = element_index * BIT_PER_ELEMENT;
        for(uint32_t i = start_index; i < start_index + BIT_PER_ELEMENT ; i++) {
            if(bitset_test(bitset, i))
                return i;
        }
        
        assert(!"This should not happen");
        return BITSET_INVALID_INDEX;
    } else {
        unsigned element_index = BITSET_INVALID_INDEX;
        for(unsigned i = 0; i < bitset->element_count; i++) {
            if(bitset->data[i] != 0xFFFFFFFF) {
                element_index = i;
                break;
            }
        }

        if(element_index == BITSET_INVALID_INDEX)
            return BITSET_INVALID_INDEX;

        uint32_t start_index = element_index * BIT_PER_ELEMENT;
        for(uint32_t i = start_index; i < start_index + BIT_PER_ELEMENT ; i++) {
            if(!bitset_test(bitset, i))
                return i;
        }
        
        assert(!"This should not happen");
        return BITSET_INVALID_INDEX;
    }

}




