#pragma once

/*
 * Return the required amount of memory for the backing storage
 * of a bitset:
 *  size_t bitset_storage_size(size_t bit_count);
 *
 * Initializes a bitset. Initializes all valid bits to unset.
 * storage must be uint32_t-aligned and sufficiently large.
 * storage may be NULL when bit_count is zero:
 *  void bitset_init(Bitset* set, uint32_t* storage, size_t bit_count);
 *
 * Set a bit. Returns false for invalid index:
 *  bool bitset_set(Bitset* set, size_t bit);
 *
 * Unset a bit. Returns false for invalid index:
 *  bool bitset_unset(Bitset* set, size_t bit);
 *
 * Test whether a bit is set. Returns false if the bit is unset or
 * the index is invalid:
 *  bool bitset_test(const Bitset* set, size_t bit);
 *
 * Find the first unset bit. Returns SIZE_MAX if all bits are set:
 *  size_t bitset_find(const Bitset* set);
 *
 * Modify count bits beginning at first.
 * Returns false for invalid range, without modifying any bits
 * Empty ranges valid when first <= bit_count:
 *  bool bitset_set_range(Bitset* set, size_t first, size_t count);
 *
 * Returns the count of unset bits
 *  size_t bitset_count_unset(const Bitset* set);
 *
 * Returns the count of bits:
 *  size_t bitset_size(const Bitset* set);
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Bitset
{
    uint32_t* words;
    size_t bit_count;
} Bitset;

static inline size_t bitset_word_count(size_t bit_count)
{
    return bit_count / 32 + (bit_count % 32 != 0);
}

static inline uint32_t bitset_low_mask(unsigned count)
{
    return count == 32 ? UINT32_MAX : (1U << count) - 1U;
}

static inline size_t bitset_storage_size(size_t bit_count)
{
    return bitset_word_count(bit_count) * sizeof(uint32_t);
}

static inline void bitset_init(Bitset* set, uint32_t* storage,
                               size_t bit_count)
{
    set->words = storage;
    set->bit_count = bit_count;

    size_t words = bitset_word_count(bit_count);
    for (size_t i = 0; i < words; ++i)
        storage[i] = 0;

    unsigned remainder = (unsigned)(bit_count % 32);
    if (remainder != 0)
        storage[words - 1] = ~bitset_low_mask(remainder);
}

static inline bool bitset_set(Bitset* set, size_t bit)
{
    if (bit >= set->bit_count)
        return false;

    set->words[bit / 32] |= 1UL << (bit % 32);
    return true;
}

static inline bool bitset_unset(Bitset* set, size_t bit)
{
    if (bit >= set->bit_count)
        return false;

    set->words[bit / 32] &= ~(1UL << (bit % 32));
    return true;
}

static inline size_t bitset_find(const Bitset* set)
{
    size_t words = bitset_word_count(set->bit_count);

    for (size_t i = 0; i < words; ++i) {
        uint32_t available = ~set->words[i];

        if (available != 0) {
            unsigned offset = 0;

            while ((available & 1UL) == 0) {
                available >>= 1;
                ++offset;
            }

            return i * 32 + offset;
        }
    }

    return SIZE_MAX;
}

static inline bool bitset_update_range(Bitset* set, size_t first, size_t count,
                                       bool value)
{
    if (first > set->bit_count || count > set->bit_count - first)
        return false;

    if (count == 0)
        return true;

    size_t word = first / 32;
    unsigned offset = (unsigned)(first % 32);

    while (count != 0) {
        unsigned chunk = 32 - offset;

        if (count < chunk)
            chunk = (unsigned)count;

        uint32_t mask = bitset_low_mask(chunk) << offset;

        if (value)
            set->words[word] |= mask;
        else
            set->words[word] &= ~mask;

        count -= chunk;
        ++word;
        offset = 0;
    }

    return true;
}

static inline bool bitset_set_range(Bitset* set, size_t first, size_t count)
{
    return bitset_update_range(set, first, count, true);
}

static inline bool bitset_unset_range(Bitset* set, size_t first, size_t count)
{
    return bitset_update_range(set, first, count, false);
}

static inline size_t bitset_count_unset(const Bitset* set)
{
    size_t total = 0;
    size_t words = bitset_word_count(set->bit_count);

    for (size_t i = 0; i < words; ++i) {
        uint32_t available = ~set->words[i];

        while (available != 0) {
            available &= available - 1UL;
            ++total;
        }
    }

    return total;
}

static inline size_t bitset_size(const Bitset* set)
{
    return set->bit_count;
}

static inline bool bitset_test(const Bitset* set, size_t bit)
{
    if (bit >= set->bit_count)
        return false;

    return (set->words[bit / 32] & (1UL << (bit % 32))) != 0;
}

