#pragma once

#include <stdint.h>

struct context {
    uint32_t    cs, ds, ss;
    uint32_t    cr3;
    uint32_t    esp, eflags, eip;
    uint32_t    edi, esi, edx, ecx, ebx, eax, ebp;
};

extern void switch_context(struct context* context);

