#include <stdint.h>
#include "random.h"

uint32_t xorshift32(uint32_t* state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}



