#pragma once

#include <stdint.h>
#include <stdbool.h>

extern uint32_t read_cr3();
extern uint32_t read_cr2();
extern uint32_t read_cr1();
extern uint32_t read_cr0();
extern uint32_t read_eflags();
extern uint32_t read_ebp();

extern void write_cr3(uint32_t val);
extern void write_cr2(uint32_t val);
extern void write_cr1(uint32_t val);
extern void write_cr0(uint32_t val);
extern void write_eflags(uint32_t val);

#define CR0_PG  (1 << 31)
#define CR0_CD  (1 << 30)
#define CR0_NW  (1 << 29)
#define CR0_AM  (1 << 18)
#define CR0_WP  (1 << 16)
#define CR0_NE  (1 << 5)
#define CR0_ET  (1 << 4)
#define CR0_TS  (1 << 3)
#define CR0_EM  (1 << 2)
#define CR0_MP  (1 << 1)
#define CR0_PE  (1)

#define CR3_PCD (1 << 4)
#define CR3_PWT (1 << 3)

#define CR4_VME (1)
#define CR4_PVI (1 << 1)
#define CR4_TSD (1 << 2)
#define CR4_DE  (1 << 3)
#define CR4_PSE (1 << 4)
#define CR4_PAE (1 << 5)
#define CR4_MCE (1 << 6)
#define CR4_PGE (1 << 7)
#define CR4_PCE (1 << 8)
#define CR4_PSFXSR      (1 << 9)
#define CR4_OSXMMEXCPT  (1 << 10)
#define CR4_VMXE        (1 << 13)
#define CR4_SMXE        (1 << 14)
#define CR4_FSGSBASE    (1 << 16)
#define CR4_PCIDE       (1 << 17)
#define CR4_OSXSAVE     (1 << 18)
#define CR4_SMEP        (1 << 20)

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

static bool interrupts_enabled() {
    uint32_t eflags = read_eflags();
    return eflags & EFLAGS_IF;
}

typedef struct {
    uint32_t esp;
    uint32_t eflags;
    uint32_t eip;
    uint32_t edi;
    uint32_t esi;
    uint32_t edx;
    uint32_t ecx;
    uint32_t ebx;
    uint32_t eax;
    uint32_t ebp;
} regs_t;

