#pragma once

#include <stdbool.h>

#define EFLAGS_CF       (1)
#define EFLAGS_PF       (1 << 2)
#define EFLAGS_AF       (1 << 4)
#define EFLAGS_ZF       (1 << 6)
#define EFLAGS_SF       (1 << 7)
#define EFLAGS_OF       (1 << 11)
#define EFLAGS_DF       (1 << 10)
#define EFLAGS_TF       (1 << 8)
#define EFLAGS_IF       (1 << 9)
#define EFLAGS_NT       (1 << 14)
#define EFLAGS_RF       (1 << 16)
#define EFLAGS_VM       (1 << 17)
#define EFLAGS_AC       (1 << 18)
#define EFLAGS_VIF      (1 << 19)
#define EFLAGS_VIP      (1 << 20)
#define EFLAGS_ID       (1 << 21)

static long read_eflags()
{
    long result;
    asm volatile("pushf\n"
                 "pop %0"
                 : "=g"(result));
    return result;
}

static inline bool interrupts_enabled()
{
    long eflags = read_eflags();
    return (eflags & EFLAGS_IF);
}

