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
unsigned hash(void* data, unsigned size)
{
    unsigned hash = 0;
    int c;
    unsigned char* ptr = data;

    while(size) {
        hash = *ptr + (hash << 6) + (hash << 16) - hash;
        ptr++;
        size--;
    }

    return hash;
}




