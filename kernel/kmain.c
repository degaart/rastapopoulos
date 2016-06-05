#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>

void outb(uint16_t port, uint8_t val) {
    asm volatile(
        "outb %%al, %%dx\n"
        :: "d"(port), "a"(val)
        : "memory"
    );
}

uint8_t inb(uint16_t port) {
    uint8_t result;
    asm volatile(
        "xor %%eax, %%eax\n"
        "inb %%dx, %%al\n"
        "movb %%al, %0\n"
        : "=r"(result)
        :
        : "eax", "edx"
    );
    return result;
}

void outw(uint16_t port, uint16_t val) {
    asm volatile(
        "outw %%ax, %%dx\n"
        :: "d"(port), "a"(val)
        : "memory"
    );    
}

uint16_t inw(uint16_t port) {
    uint16_t result;
    asm volatile(
        "xor %%eax, %%eax\n"
        "inw %%dx, %%ax\n"
        "movw %%ax, %0\n"
        : "=r"(result)
        :
        : "eax", "edx"
    );
    return result;
}



void kmain()
{
    const char* str = "Hello, from C\r\n";
    while(*str) {
        outb(0xE9, *str);
        str++;
    }

    asm volatile(
            ".intel_syntax noprefix\n"
            "int 0x13\n"
    );
}

