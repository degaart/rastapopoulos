#include "bitset.h"
#include "kmalloc.h"
#include "string.h"
#include "debug.h"

#define MAGIC 0xCCCCCCCC

#define CHECK_OFFSET(bitset,offset) \
    do { \
        if(offset < 0 || offset >= (bitset)->bitcount) { \
            panic("Invalid bitset offset %d (bitcount: %d)", offset, (bitset)->bitcount); \
        } \
    } while(0)

#define CHECK(bitset) \
    do { \
        assert((bitset)->cookie == MAGIC); \
        assert((bitset)->data[(bitset)->count] == MAGIC); \
    } while(0)

struct bitset* bitset_alloc(size_t count)
{
    struct bitset* result = kmalloc(BITSET_ALLOC_SIZE(count));
    return bitset_init(result, count);
}

struct bitset* bitset_init(struct bitset* bitset, size_t count)
{
    bitset->bitcount = count;
    bitset->count = BITSET_ALIGN_COUNT(count);
    memset(bitset->data, 0, sizeof(unsigned long) * bitset->count);
    bitset->cookie = MAGIC;
    bitset->data[bitset->count] = MAGIC;

    assert((bitset->count * BITS_PER_ELEMENT) >= count);
    return bitset;
}

bool bitset_test(const struct bitset* bitset, size_t offset)
{
    CHECK_OFFSET(bitset, offset);
    CHECK(bitset);

    size_t index = offset / BITS_PER_ELEMENT;
    unsigned long mask = 1 << (offset % BITS_PER_ELEMENT);
    return bitset->data[index] & mask;
}

void bitset_set(struct bitset* bitset, size_t offset)
{
    CHECK_OFFSET(bitset, offset);
    CHECK(bitset);

    size_t index = offset / BITS_PER_ELEMENT;
    unsigned long mask = 1 << (offset % BITS_PER_ELEMENT);
    assert(!(bitset->data[index] & mask));
    bitset->data[index] |= mask;
}

void bitset_set_all(struct bitset* bitset)
{
    CHECK(bitset);
    for(size_t i = 0; i < bitset->count; i++)
        bitset->data[i] = 0xFFFFFFFF;
}

void bitset_clear(struct bitset* bitset, size_t offset)
{
    CHECK_OFFSET(bitset, offset);
    CHECK(bitset);

    size_t index = offset / BITS_PER_ELEMENT;
    unsigned long mask = 1 << (offset % BITS_PER_ELEMENT);
    assert(bitset->data[index] & mask);
    bitset->data[index] &= ~mask;
}

void bitset_clear_all(struct bitset* bitset)
{
    CHECK(bitset);
    for(size_t i = 0; i < bitset->count; i++)
        bitset->data[i] = 0;
}

void bitset_set_range(struct bitset* bitset, size_t offset, size_t len)
{
    for(size_t i = offset; i < offset + len; i++)
        bitset_set(bitset, i);
}

void test_bitset()
{
    trace(" -= Testing bitset =-");

    /* should allocate 5 unsigned longs */
    struct bitset* b = bitset_alloc(130);
    assert(b->bitcount == 130);
    assert(b->count == 5);
    assert(b->cookie == 0xCCCCCCCC);
    assert(b->data[5] == 0xCCCCCCCC);

    for(size_t i = 0; i < 130; i++) {
        assert(!bitset_test(b, i));
    }

    bitset_set(b, 0);
    assert(bitset_test(b, 0));
    for(size_t i = 1; i < 130; i++) {
        assert(!bitset_test(b, i));
    }

    bitset_set(b, 32);
    assert(bitset_test(b, 0));
    assert(bitset_test(b, 32));
    for(size_t i = 1; i < 130; i++) {
        if(i != 32)
            assert(!bitset_test(b, i));
    }

    bitset_set(b, 129);
    for(size_t i = 0; i < 130; i++) {
        if(i == 0 || i == 32 || i == 129)
            assert(bitset_test(b, i));
        else
            assert(!bitset_test(b, i));
    }

    bitset_set_all(b);
    for(size_t i = 0; i < 130; i++)
        assert(bitset_test(b, i));

    bitset_clear_all(b);
    for(size_t i = 0; i < 130; i++) {
        assert(!bitset_test(b, i));
    }

    bitset_set(b, 7);
    bitset_set(b, 29);
    bitset_set(b, 100);
    bitset_set(b, 128);
    for(size_t i = 0; i < 130; i++) {
        if(i == 7 || i == 29 || i == 100 || i == 128)
            assert(bitset_test(b, i));
        else
            assert(!bitset_test(b, i));
    }

    bitset_clear_all(b);
    bitset_set_range(b, 42, 59);
    for(size_t i = 0; i < 130; i++) {
        if(i >= 42 && i <= 100) {
            assert(bitset_test(b, i));
        } else {
            assert(!bitset_test(b, i));
        }
    }

    bitset_set_range(b, 127, 3);
    for(size_t i = 0; i < 130; i++) {
        if(i >= 42 && i <= 100) {
            assert(bitset_test(b, i));
        } else if(i >= 127) {
            assert(bitset_test(b, i));
        } else {
            assert(!bitset_test(b, i));
        }
    }

    bitset_clear(b, 42);
    for(size_t i = 0; i < 130; i++) {
        if(i == 42) {
            assert(!bitset_test(b, i));
        } else if(i >= 43 && i <= 100) {
            assert(bitset_test(b, i));
        } else if(i >= 127) {
            assert(bitset_test(b, i));
        } else {
            assert(!bitset_test(b, i));
        }
    }

    trace(" -= Done testing bitset =-");
}

