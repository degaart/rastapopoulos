#pragma once

#include <stdint.h>
#include <stdbool.h>

struct bitset {
    uint32_t size;
    uint32_t element_count;
    uint32_t data[];
};

#define BITSET_INVALID_INDEX 0xFFFFFFFF

struct bitset* bitset_new(uint32_t size);
unsigned bitset_alloc_size(uint32_t size);
bool bitset_test(const struct bitset* bitset, uint32_t offset);
void bitset_set(struct bitset* bitset, uint32_t offset);
void bitset_set_all(struct bitset* bitset);
void bitset_clear(struct bitset* bitset, uint32_t offset);
void bitset_clear_all(struct bitset* bitset);
void bitset_set_range(struct bitset* bitset, uint32_t offset, uint32_t len);
uint32_t bitset_find(struct bitset* bitset, unsigned value); /* Returns BITSET_INVALID_INDEX if not found */

