#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BITSET_INVALID 0xFFFFFFFF

struct bitset;

size_t bitset_get_size(unsigned count);
void bitset_init(struct bitset* bitset, size_t size, unsigned count);
void bitset_set_value(struct bitset* bitset, unsigned index, bool value);
bool bitset_get(const struct bitset* bitset, unsigned index);
void bitset_set(struct bitset* bitset, unsigned index);
void bitset_clear(struct bitset* bitset, unsigned index);
size_t bitset_find(struct bitset* bitset);

