#include "rbuf.h"
#include "debug.h"

void test_rbuf(void)
{
    unsigned buffer[5] = {0};
    RBUF_DECLARE(uint_rbuf, unsigned);
    uint_rbuf_t rbuf;
    RBUF_INIT(&rbuf, buffer, 4);
    RBUF_PUSH(&rbuf, 0x01010101);
    assert(buffer[0] == 0x01010101);
    assert(buffer[1] == 0);
    assert(buffer[2] == 0);
    assert(buffer[3] == 0);
    assert(buffer[4] == 0);

    RBUF_PUSH(&rbuf, 0x02020202);
    assert(buffer[0] == 0x01010101);
    assert(buffer[1] == 0x02020202);
    assert(buffer[2] == 0);
    assert(buffer[3] == 0);
    assert(buffer[4] == 0);

    RBUF_PUSH(&rbuf, 0x03030303);
    assert(buffer[0] == 0x01010101);
    assert(buffer[1] == 0x02020202);
    assert(buffer[2] == 0x03030303);
    assert(buffer[3] == 0);
    assert(buffer[4] == 0);

    bool ret = RBUF_TRY_PUSH(&rbuf, 0x04040404);
    assert(ret == false);
    assert(buffer[0] == 0x01010101);
    assert(buffer[1] == 0x02020202);
    assert(buffer[2] == 0x03030303);
    assert(buffer[3] == 0);
    assert(buffer[4] == 0);

    RBUF_PUSH(&rbuf, 0x04040404);
    assert(buffer[0] == 0x01010101);
    assert(buffer[1] == 0x02020202);
    assert(buffer[2] == 0x03030303);
    assert(buffer[3] == 0x04040404);
    assert(buffer[4] == 0);

    RBUF_PUSH(&rbuf, 0x05050505);
    assert(buffer[0] == 0x05050505);
    assert(buffer[1] == 0x02020202);
    assert(buffer[2] == 0x03030303);
    assert(buffer[3] == 0x04040404);
    assert(buffer[4] == 0);

    assert(RBUF_POP(&rbuf, 0xCCCCCCCC) == 0x03030303);
    assert(RBUF_POP(&rbuf, 0xCCCCCCCC) == 0x04040404);
    assert(RBUF_POP(&rbuf, 0xCCCCCCCC) == 0x05050505);
    assert(RBUF_POP(&rbuf, 0xCCCCCCCC) == 0xCCCCCCCC);

    unsigned storage;
    assert(RBUF_TRY_POP(&rbuf, &storage) == NULL);

    RBUF_PUSH(&rbuf, 0x06060606);
    assert(RBUF_POP(&rbuf, 0xCCCCCCCC) == 0x06060606);
}
