#pragma once

#include <stdint.h>

struct int10_regs {
    uint32_t eax, ebx, ecx, edx;
    uint32_t ebp, esi, edi;
    uint32_t es, fs, gs;
    uint32_t reserved;
} __attribute((packed));

void int10(struct int10_regs*);
void int10_init();


