#pragma once

#include <stdint.h>

static inline __attribute__((always_inline)) uint8_t inb(uint16_t port)
{
    uint8_t value;

    __asm__ volatile("in %0, %1" : "=a"(value) : "Nd"(port));

    return value;
}

static inline __attribute__((always_inline)) void outb(uint16_t port,
                                                       uint8_t value)
{
    __asm__ volatile("out %1, %0" : : "a"(value), "Nd"(port));
}

