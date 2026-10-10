#pragma once

#include <stdint.h>

#define CR0_PE 0x1
#define CR0_PG 0x80000000

extern uint8_t __kernel_start[];
extern uint8_t __kernel_end[];

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
    __asm__ volatile("mov %0, cr0" : "=r"(value));
    return value;
}

static inline uint32_t read_cr2(void)
{
    uint32_t value;
    __asm__ volatile("mov %0, cr2" : "=r"(value));
    return value;
}

static inline uint32_t read_cr3(void)
{
    uint32_t value;
    __asm__ volatile("mov %0, cr3" : "=r"(value));
    return value;
}

static inline uint16_t read_ss(void)
{
    uint16_t value;
    __asm__ volatile("mov %0, ss" : "=r"(value));
    return value;
}

static inline void write_cr0(uint32_t value)
{
    __asm__ volatile("mov cr0, %0" : : "r"(value) : "memory");
}

static inline void write_cr3(uint32_t value)
{
    __asm__ volatile("mov cr3, %0" : : "r"(value) : "memory");
}

static inline void io_wait(void)
{
    outb(0x80, 0);
}

#define panic(...) __panic(__FILE__, __LINE__, __VA_ARGS__)

__attribute__((format(printf, 3, 4))) void __panic(const char* file, int line,
                                                   const char* fmt, ...);

extern void halt(void) __attribute__((noreturn));

