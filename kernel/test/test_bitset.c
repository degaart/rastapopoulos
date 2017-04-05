#include "../bitset.h"
#include "../debug.h"
#include "../kmalloc.h"

void test_bitset()
{
    trace("Testing bitset");

    // Create new bitset
    // should be 4 elements
    int alloc_size = bitset_alloc_size(120);
    struct bitset* bt = bitset_init(kmalloc(alloc_size), 120);  
    assert(bt->element_count == 4);

    // On creation, all bits should be unset
    for(int i = 0; i < 120; i++) {
        assert(!bitset_test(bt, i));
    }

    // And all elements should be zero
    for(int i = 0; i < 4; i++) {
        assert(bt->data[i] == 0);
    }

    // Set all bits, and test all bits and elements are set
    bitset_set_all(bt);
    for(int i = 0; i < 120; i++) {
        assert(bitset_test(bt, i));
    }
    for(int i = 0; i < 4; i++) {
        assert(bt->data[i] == 0xFFFFFFFF);
    }

    // Reset
    bitset_clear_all(bt);

    // Set bit #0 and test
    bitset_set(bt, 0);
    assert(bitset_test(bt, 0));
    assert(bt->data[0] == 0x1);

    // Set bit #31 and test
    bitset_set(bt, 31);
    assert(bitset_test(bt, 31));
    assert(bt->data[0] == 0x80000001);

    // Set bit 32 and test
    bitset_set(bt, 32);
    assert(bitset_test(bt, 32));
    assert(bt->data[0] == 0x80000001);
    assert(bt->data[1] == 0x1);

    // Set bit 119 and test
    bitset_set(bt, 119);
    assert(bitset_test(bt, 119));
    assert(bt->data[0] == 0x80000001);
    assert(bt->data[1] == 0x1);
    assert(bt->data[3] == 1 << 23);

    // Now, unset bit 119
    bitset_clear(bt, 119);
    assert(!bitset_test(bt, 119));
    assert(bt->data[0] == 0x80000001);
    assert(bt->data[1] == 0x1);
    assert(bt->data[3] == 0);

    // Setting bit 120 should trigger an assert failure
    //bitset_clear(bt, 120);
}


