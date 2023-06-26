#include <stdint.h>
#include <stddef.h>

static size_t strlen(const char* s)
{
    size_t ret = 0;
    while(*(s++))
        ret++;
    return ret;
}

void start()
{
    const char* message = "All your base are belong to us\r\n";
    uint16_t len = strlen(message);

    uint16_t dx;
    asm volatile(
        "mov  ah, 0x03\n"
        "xor  bh, bh\n"
        "int  0x10\n"
        : "=d"(dx)
        :
        : "cx");

    asm volatile(
        "push ebp\n"
        "mov  bp, %0\n"
        "int  0x10\n"
        "pop  ebp\n"
        :
        : "g"((uint16_t)(uintptr_t)message),
          "a"((uint16_t)0x1301),
          "b"((uint16_t)0x0007),
          "c"(len),
          "d"(dx));
    while(1);
}

