#include "../locks.h"
#include "../debug.h"

static void test_cmpxchg()
{
    trace("Testing cmpxchg");

    volatile uint32_t lock = 0;

    uint32_t ret = cmpxchg(&lock, 1, 0);
    assert(ret == 0);
    assert(lock == 1);

    ret = cmpxchg(&lock, 0, 1);
    assert(ret == 1);
    assert(lock == 0);

    ret = cmpxchg(&lock, 0, 1);
    assert(ret == 0);
    assert(lock == 0);
}

void test_locks()
{
    trace("Testing locks");
    test_cmpxchg();
}


