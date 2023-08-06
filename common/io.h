#pragma once

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val)
{
    asm volatile("out %1, %0" : : "a"(val), "Nd"(port) : "memory");
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t result;
    asm volatile("in %0, %1" : "=a"(result) : "Nd"(port) : "memory");
    return result;
}

static inline void outw(uint16_t port, uint16_t val)
{
    asm volatile("out %1, %0" ::"a"(val), "Nd"(port) : "memory");
}

static inline uint16_t inw(uint16_t port)
{
    uint16_t result;
    asm volatile("in %0, %1" : "=a"(result) : "Nd"(port) : "memory");
    return result;
}

static inline void outl(uint16_t port, uint32_t val)
{
    asm volatile("out %1, %0" ::"a"(val), "Nd"(port) : "memory");
}

static inline uint32_t inl(uint16_t port)
{
    uint32_t result;
    asm volatile("in %0, %1" : "=a"(result) : "Nd"(port) : "memory");
    return result;
}

static inline void io_wait(void)
{
    outb(0x80, 0);
}
