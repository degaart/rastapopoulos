#ifndef _BITSET_H_
#define _BITSET_H_

#include <stdint.h>
#include <limits.h>

class Bitset {
private:
    static const int MAGIC = 0xB17537;
    unsigned _magic;
    
    static const int BITS_PER_ELEMENT = sizeof(unsigned) * 8;
    
    unsigned *_data;                                    /* data */
    unsigned _size;                                     /* total number of bits */
    unsigned _data_size;                                /* data size in bytes */
public:
    Bitset(unsigned size);                              /* size: number of bits to store */
    Bitset() = delete;
    Bitset(const Bitset&);
    ~Bitset();

    Bitset& operator=(const Bitset&) = delete;
    
    bool test(unsigned index) const;
    void set(unsigned index);
    void set_range(unsigned index, unsigned size);
    void fill();                                        /* Set all bits */
    
    void clear(unsigned index);
    void clear_range(unsigned index, unsigned size);
    void clear();
    
    unsigned find() const;                                    /* get index of first unset bit, or UINT_MAX of no unset bit found */
    unsigned find_range(unsigned size) const;                 /* find unset contiguous range of bits or UINT_MAX of no unset bit found */
};

#endif /* defined(_BITSET_H_) */
