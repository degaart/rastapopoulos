#include "util.h"

extern void real_rdtsc(uint32_t* lo, uint32_t* hi);

uint64_t rdtsc()
{
    uint32_t lo, hi;
    real_rdtsc(&lo, &hi);
    uint64_t result = MAKE_UINT64(lo, hi);
    return result;
}

