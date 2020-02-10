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

int main()
{
    while(1) {
        puts("Hello from userspace");
        for(size_t i = 0; i < 1000000; i++) {
            io_wait();
        }
    }
        
    return 0;
}

