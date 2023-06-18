#pragma once
#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val)
{
    asm volatile(
            "outb %0, %1"
            :
            : "a"(val), "Nd"(port)
            : "memory");
}

static inline void and_eflags(uint32_t mask)
{
    asm volatile(
            "pushfl\n"
            "pop %%eax\n"
            "and %0, %%eax\n"
            "push %%eax\n"
            "popfl\n"
            : "=r"(mask)
            :
            : "eax", "memory");
}

