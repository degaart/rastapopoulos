#ifndef _UTIL_H_
#define _UTIL_H_

#define halt()  \
    while(1) { \
    	asm volatile(	\
    		".intel_syntax noprefix\n" \
    		"xchg bx, bx\n" \
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


#ifndef __APPLE__
#define write_cr3(x)    asm volatile("mov %0, %%cr3" :: "r"(x))
#define write_cr2(x)    asm volatile("mov %0, %%cr2" :: "r"(x))
#define write_cr1(x)    asm volatile("mov %0, %%cr1" :: "r"(x))
#define write_cr0(x)    asm volatile("mov %0, %%cr0" :: "r"(x))

#define read_cr0(x)     asm volatile("mov %%cr0, %0" : "=r"(x))
#define read_cr1(x)     asm volatile("mov %%cr1, %0" : "=r"(x))
#define read_cr2(x)     asm volatile("mov %%cr2, %0" : "=r"(x))
#define read_cr3(x)     asm volatile("mov %%cr3, %0" : "=r"(x))
#else
#define write_cr3(x)
#define write_cr2(x)
#define write_cr1(x)
#define write_cr0(x)

#define read_cr0(x)
#define read_cr1(x)
#define read_cr2(x)
#define read_cr3(x)
#endif

;   /* To make sublime text happy */

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
