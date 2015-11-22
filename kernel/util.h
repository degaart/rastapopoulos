#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdint.h>

#define yield() \
    asm volatile(   \
        ".intel_syntax noprefix\n"      \
        "hlt\n"                         \
        ::: "memory" \
    )

#define halt()      \
    cli();          \
    while(1) {      \
    	yield();    \
    }

#define pushf() \
    asm volatile( \
        ".intel_syntax noprefix\n" \
        "pushf\n" \
        ::: "memory" \
    )

#define popf() \
    asm volatile( \
        ".intel_syntax noprefix\n" \
        "popf\n" \
        ::: "memory" \
    )

#define sti() \
    asm volatile( \
        ".intel_syntax noprefix\n" \
        "sti\n" \
        ::: "memory" \
    )

#define cli() \
    asm volatile( \
        ".intel_syntax noprefix\n" \
        "cli\n" \
        ::: "memory" \
    )

#define EnterCriticalSection(handle) \
    read_eflags(handle); \
    handle &= EFLAGS_IF; \
    cli()

#define LeaveCriticalSection(handle) \
    if(handle & EFLAGS_IF) \
        sti(); \
    else \
        cli() 

    

#include "regs.h"

;   /* To make sublime text happy */

#define LINKER_SYMBOL(sym) extern unsigned char sym[]

LINKER_SYMBOL(_TEXT_START_);
LINKER_SYMBOL(_TEXT_END_);
LINKER_SYMBOL(_RODATA_START_);
LINKER_SYMBOL(_RODATA_END_);
LINKER_SYMBOL(_DATA_START_);
LINKER_SYMBOL(_DATA_END_);
LINKER_SYMBOL(_CTORS_START_);
LINKER_SYMBOL(_CTORS_END_);
LINKER_SYMBOL(_BSS_START_);
LINKER_SYMBOL(_BSS_END_);
LINKER_SYMBOL(_KERNEL_END_);

#define EXPORT extern "C"

#include "vmm.h"

template<typename T> T align(T value, unsigned alignment = VMM::PAGE_SIZE) {
    unsigned result = (unsigned)value;
    result += alignment - 1;
    result &= ~(alignment - 1);
    return (T)result;
}

template<typename T> T truncate(T value, unsigned alignment = VMM::PAGE_SIZE) {
    return (value / alignment) * alignment;
}

template<typename T> bool is_aligned(T value, unsigned alignment = VMM::PAGE_SIZE) {
    return (((unsigned)value) % alignment) == 0;
}

#define LOBYTE(i) ((i) & 0xFF)
#define HIBYTE(i) (((i) & 0xFF00) >> 8)

#define LOWORD(i)   ((i) & 0xFFFF)
#define HIWORD(i)   (((i) & 0xFFFF0000) >> 16)

#define LODWORD(i)  ((uint32_t) ((i) & 0xFFFFFFFF))
#define HIDWORD(i)  ((uint32_t) (((i) & 0xFFFFFFFF00000000) >> 32))

#define MAKE_UINT64(lo, hi)  ( ((uint64_t)(lo)) | ((uint64_t)(hi) << 32) )

#include <stdint.h>

class Util;

class Random {
private:
    friend Util;
    
    uint32_t _seed;
public:
    Random(uint32_t seed);
    uint32_t next();
};

class Util {
private:
    static Random _shared_random;
public:
    static void srand(uint32_t seed);
    static uint32_t rand();
    static uint32_t crc32(const void *s, unsigned len, uint32_t startval = 0);
};

#endif
