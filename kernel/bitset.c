#include "bitset.h"

static size_t bitset_word_count(size_t bit_count)
{
    return bit_count / 32 + (bit_count % 32 != 0);
}

/* count must be in the range 0..32. */
static uint32_t bitset_low_mask(unsigned count)
{
    return count == 32 ? UINT32_MAX : (1U << count) - 1U;
}

size_t bitset_storage_size(size_t bit_count)
{
    return bitset_word_count(bit_count) * sizeof(uint32_t);
}

void bitset_init(Bitset* set, uint32_t* storage, size_t bit_count)
{
    set->words = storage;
    set->bit_count = bit_count;

    size_t words = bitset_word_count(bit_count);

    for (size_t i = 0; i < words; ++i)
        storage[i] = 0;

    /* Exclude unused bits from searches and unset-bit counts. */
    unsigned remainder = (unsigned)(bit_count % 32);

    if (remainder != 0)
        storage[words - 1] = ~bitset_low_mask(remainder);
}

bool bitset_set(Bitset* set, size_t bit)
{
    if (bit >= set->bit_count)
        return false;

    set->words[bit / 32] |= 1UL << (bit % 32);
    return true;
}

bool bitset_unset(Bitset* set, size_t bit)
{
    if (bit >= set->bit_count)
        return false;

    set->words[bit / 32] &= ~(1UL << (bit % 32));
    return true;
}

size_t bitset_find(const Bitset* set)
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

static bool bitset_update_range(Bitset* set, size_t first, size_t count,
                                bool value)
{
    /* Validate without overflowing first + count. */
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

bool bitset_set_range(Bitset* set, size_t first, size_t count)
{
    return bitset_update_range(set, first, count, true);
}

bool bitset_unset_range(Bitset* set, size_t first, size_t count)
{
    return bitset_update_range(set, first, count, false);
}

size_t bitset_count_unset(const Bitset* set)
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

size_t bitset_size(const Bitset* set)
{
    return set->bit_count;
}

bool bitset_test(const Bitset* set, size_t bit)
{
    if (bit >= set->bit_count)
        return false;

    return (set->words[bit / 32] & (1UL << (bit % 32))) != 0;
}

