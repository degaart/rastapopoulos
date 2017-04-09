#include "util.h"

extern void real_rdtsc(uint32_t* lo, uint32_t* hi);

uint64_t rdtsc()
{
    uint32_t lo, hi;
    real_rdtsc(&lo, &hi);
    uint64_t result = MAKE_UINT64(lo, hi);
    return result;
}

/*
 * hash function from sbdm
 */
unsigned hash2(const void* data, unsigned size, unsigned start_hash)
{
    unsigned hash = start_hash;
    int c;
    const unsigned char* ptr = data;

    while(size) {
        hash = *ptr + (hash << 6) + (hash << 16) - hash;
        ptr++;
        size--;
    }

    return hash;
}


unsigned hash(const void* data, unsigned size)
{
    return hash2(data, size, 0);
}

