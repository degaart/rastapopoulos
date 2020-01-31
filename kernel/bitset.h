#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "util.h"

struct bitset {
    size_t count;           /* count of unsigned long's in data */
    size_t bitcount;        /* number of valid bits */
    unsigned long cookie;
    unsigned long data[];
};

#define BITSET_INVALID_INDEX 0xFFFFFFFF

#define BITS_PER_ELEMENT (sizeof(unsigned long)*8)
#define BITSET_ALIGN_COUNT(count) \
    (ALIGN(count, BITS_PER_ELEMENT) / BITS_PER_ELEMENT)
#define BITSET_ALLOC_SIZE(count) \
    ((sizeof(struct bitset) + \
      (sizeof(unsigned long)*BITSET_ALIGN_COUNT(count))) + 1)

struct bitset* bitset_alloc(size_t count);
struct bitset* bitset_init(struct bitset* bitset, size_t count); /* Assume bitset has been allocated properly */
void bitset_free(struct bitset* bitset);
bool bitset_test(const struct bitset* bitset, size_t offset);
void bitset_set(struct bitset* bitset, size_t offset);
void bitset_set_all(struct bitset* bitset);
void bitset_clear(struct bitset* bitset, size_t offset);
void bitset_clear_all(struct bitset* bitset);
void bitset_set_range(struct bitset* bitset, size_t offset, size_t len);

void test_bitset();


