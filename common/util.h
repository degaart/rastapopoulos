#pragma once

#include <stdint.h>
#include <stdbool.h>

#define LOBYTE(i) ((i) & 0xFF)
#define HIBYTE(i) (((i) & 0xFF00) >> 8)

#define LOWORD(i)   ((i) & 0xFFFF)
#define HIWORD(i)   (((i) & 0xFFFF0000) >> 16)

#define LODWORD(i)  ((uint32_t) ((i) & 0xFFFFFFFF))
#define HIDWORD(i)  ((uint32_t) (((i) & 0xFFFFFFFF00000000) >> 32))

#define BITTEST(var, idx)   ((var) & (1 << (idx)))
#define BITSET(var, idx)    (var) |= (1 << (idx))
#define BITCLEAR(var, idx)    (var) &= (1 << (~(idx)))

#define MAKE_UINT64(lo, hi)  ( ((uint64_t)(lo)) | ((uint64_t)(hi) << 32) )

#define countof(a) sizeof(a) / sizeof(a[0])

/* WARNING: This macro only works with powers of two */
#define ALIGN(val, align) \
    ((typeof(val)) \
    (( (uint32_t)(val) + ((uint32_t)(align) - 1) ) & ~( (uint32_t)(align) - 1) ))

#define TRUNCATE(val, align) \
    (((val) / (align)) * (align))

#define IS_ALIGNED(val, align) \
    ((typeof(val)) \
    (( ((uint32_t)(val)) % ((uint32_t)(align)) ) == 0))

uint64_t rdtsc();
unsigned hash2(const void* data, unsigned size, unsigned start_hash);
unsigned hash(const void* data, unsigned size);


#define dump_var(var) \
    trace("%s: %p", #var, var)

/* http://graphics.stanford.edu/~seander/bithacks.html */
static bool is_pow2(unsigned v)
{
    return v && !(v & (v - 1));
}

static int log2(unsigned v)
{
    int r;      // result goes here

    static const int MultiplyDeBruijnBitPosition[32] = 
    {
      0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30,
      8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31
    };

    v |= v >> 1; // first round down to one less than a power of 2 
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;

    r = MultiplyDeBruijnBitPosition[(uint32_t)(v * 0x07C4ACDDU) >> 27];
    return r;
}

static unsigned next_pow2(unsigned v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

