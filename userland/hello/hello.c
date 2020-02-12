#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define DEBUG_PORT 0xE9

static inline void outb(uint16_t port, uint8_t value)
{
    asm volatile("out %0,%1" : : "dN"(port), "a" (value));
}

static inline void io_wait()
{
    outb(0x80, 0);
}

static void puts(const char* str)
{
    while(*str) {
        outb(0xE9, *str);
        str++;
    }
}

static inline void yield()
{
    asm volatile("\tint 0x80\n":::"memory");
}

int main()
{
    while(1) {
        puts("hello.elf running\n");
        yield();
    }
    return 0;
}

