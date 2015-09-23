#ifndef _UTIL_H_
#define _UTIL_H_

#define halt()  \
    while(1) { \
    	asm volatile(	\
    		".intel_syntax noprefix\n" \
            "cli\n" \
            "hlt\n" \
    	); \
    }

#define pushf() \
    asm volatile( \
        ".intel_syntax noprefix\n" \
        "pushf\n" \
    )

#define popf() \
    asm volatile( \
        ".intel_syntax noprefix\n" \
        "popf\n" \
    )

#define sti() \
    asm volatile( \
        ".intel_syntax noprefix\n" \
        "sti\n" \
    )

#define cli() \
    asm volatile( \
        ".intel_syntax noprefix\n" \
        "cli\n" \
    )


#include "regs.h"

;   /* To make sublime text happy */

#define LINKER_SYMBOL(sym) extern unsigned char sym[]

LINKER_SYMBOL(_TEXT_START_);
LINKER_SYMBOL(_TEXT_START_);
LINKER_SYMBOL(_RODATA_START_);
LINKER_SYMBOL(_RODATA_END_);
LINKER_SYMBOL(_DATA_START_);
LINKER_SYMBOL(_DATA_END_);
LINKER_SYMBOL(_CTORS_START_);
LINKER_SYMBOL(_CTORS_END_);
LINKER_SYMBOL(_BSS_START_);
LINKER_SYMBOL(_BSS_END_);
LINKER_SYMBOL(_KERNEL_END_);

template<typename T> T align(T value, unsigned alignment) {
    unsigned result = (unsigned)value;
    result += alignment - 1;
    result &= ~(alignment - 1);
    return (T)result;
}

template<typename T> T truncate(T value, unsigned alignment) {
    return (value / alignment) * alignment;
}


#endif
