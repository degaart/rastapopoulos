#include "bitset.h"
#include "debug.h"
#include "string.h"
#include "cxxutil.h"

Bitset::Bitset(unsigned size)
: _size(size) {
    _data_size = (size / BITS_PER_ELEMENT) + 1;
    _data = new unsigned[_data_size];
}

Bitset::Bitset(const Bitset& bitset)
: _size(bitset._size), _data_size(bitset._data_size) {
    _data = new unsigned[_data_size];
    memcpy(_data, bitset._data, _data_size * sizeof(unsigned));
}

Bitset::~Bitset() {
    delete [] _data;
}

bool Bitset::test(unsigned index) const {
    assert(index < _size);
    
    unsigned byte_index = index / BITS_PER_ELEMENT;
    unsigned bit_offset = index % BITS_PER_ELEMENT;
    assert(byte_index < _data_size);
    assert(bit_offset < BITS_PER_ELEMENT);
    
    return _data[byte_index] & (1 << bit_offset);
}

void Bitset::set(unsigned index) {
    assert(index < _size);
    
    unsigned byte_index = index / BITS_PER_ELEMENT;
    unsigned bit_offset = index % BITS_PER_ELEMENT;
    assert(byte_index < _data_size);
    assert(bit_offset < BITS_PER_ELEMENT);
    
    _data[byte_index] |= (1 << bit_offset);
}

void Bitset::set_range(unsigned index, unsigned size) {
    for(unsigned i = index; i < index + size; i++)
        set(i);
}

void Bitset::fill() {
    memset(_data, UINT_MAX, _data_size * sizeof(unsigned));
}

void Bitset::clear(unsigned index) {
    assert(index < _size);
    
    unsigned byte_index = index / BITS_PER_ELEMENT;
    unsigned bit_offset = index % BITS_PER_ELEMENT;
    assert(byte_index < _data_size);
    assert(bit_offset < BITS_PER_ELEMENT);
    
    _data[byte_index] &= ~(1 << bit_offset);
}

void Bitset::clear_range(unsigned index, unsigned size) {
    for(unsigned i = index; i < index + size; i++)
        clear(i);
}

void Bitset::clear() {
    bzero(_data, _data_size * sizeof(unsigned));
}

unsigned Bitset::find() const {
    for(unsigned i = 0; i < _data_size; i++) {
        if(_data[i] != UINT_MAX) {
            for(unsigned bit = 0; bit < BITS_PER_ELEMENT; bit++) {
                unsigned mask = 1 << bit;
                if(!(_data[i] & mask)) {
                    if( (i * BITS_PER_ELEMENT) + bit < _size)
                        return (i * BITS_PER_ELEMENT) + bit;
                    else
                        break;
                }
            }
        }
    }
    return UINT_MAX;
}

unsigned Bitset::find_range(unsigned size) const {
    if( size == 1)
        return find();
    
    for(unsigned i = 0; i < _data_size; i++) {
        if(_data[i] != UINT_MAX) {
            for(unsigned bit = 0; bit < BITS_PER_ELEMENT; bit++) {
                unsigned mask = 1 << bit;
                if(!(_data[i] & mask)) {
                    unsigned index = (i * BITS_PER_ELEMENT) + bit;
                    
                    bool free = true;
                    for(unsigned i = index; (i < index + size) && (i < _size); i++) {
                        if(test(i)) {
                            free = false;
                            break;
                        }
                    }
                    
                    if(free)
                        return index;
                }
            }
        }
    }
    
    return UINT_MAX;
}

