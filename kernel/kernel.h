#pragma once

#include <stdint.h>

/* A must be a power of two */
#define ALIGN_UP(V, A)   (((V) + (A) - 1) & (~((A) - 1)))
#define ALIGN_DOWN(V, A) ((V) & ~((A) - 1))

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

static inline uint32_t read_cr0(void)
{
    uint32_t value;
    __asm__ volatile("mov %%cr0, %0" : "=r"(value));
    return value;
}

static inline uint32_t read_cr2(void)
{
    uint32_t value;
    __asm__ volatile("mov %%cr2, %0" : "=r"(value));
    return value;
}

static inline uint16_t read_ss(void)
{
    uint16_t value;
    __asm__ volatile("mov %%ss, %0" : "=r"(value));
    return value;
}

#define panic(...) __panic(__FILE__, __LINE__, __VA_ARGS__)

__attribute__((format(printf, 3, 4))) void __panic(const char* file, int line,
                                                   const char* fmt, ...);

