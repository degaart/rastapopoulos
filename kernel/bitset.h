#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint32_t* words;
    size_t bit_count;
} Bitset;

/* Required backing storage in bytes, excluding the descriptor */
size_t bitset_storage_size(size_t bit_count);

/*
 * Initializes all valid bits to unset
 * storage must be uint32_t-aligned and sufficiently large
 * storage may be NULL when bit_count is zero
 */
void bitset_init(Bitset* set, uint32_t* storage, size_t bit_count);

/* Return false for an out-of-bounds bit index */
bool bitset_set(Bitset* set, size_t bit);
bool bitset_unset(Bitset* set, size_t bit);

/* Returns false if the bit is unset or the index is out of bounds */
bool bitset_test(const Bitset* set, size_t bit);

/*
 * Find the first unset bit
 * Returns SIZE_MAX if no unset bit exists
 */
size_t bitset_find(const Bitset* set);

/*
 * Modify 'count' bits beginning at 'first'
 * Return false for an invalid range, without modifying any bits
 * Empty ranges are valid when first <= bit_count
 */
bool bitset_set_range(Bitset* set, size_t first, size_t count);
bool bitset_unset_range(Bitset* set, size_t first, size_t count);

size_t bitset_count_unset(const Bitset* set);
size_t bitset_size(const Bitset* set);

