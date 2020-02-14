#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <cpuid.h>

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

static unsigned long read_eflags()
{
    unsigned long result;

    /* memory clobber to force ordering */
    asm volatile("pushf\n"
                 "pop %0"
                 : "=g"(result)
                 :: "memory");
    return result;
}

static void write_eflags(unsigned long flags)
{
    /* cc clober because we modify condition flags */
    asm ("push %0\n\tpopf" : : "rm"(flags) : "memory","cc");
}

static inline bool interrupts_enabled()
{
    unsigned long eflags = read_eflags();
    return (eflags & EFLAGS_IF);
}

static inline void lidt(void* base, uint16_t size)
{   
    // This function works in 32 and 64bit mode
    struct {
        uint16_t length;
        void*    base;
    } __attribute__((packed)) IDTR = { size, base };

    asm ( "lidt %0" : : "m"(IDTR) );  // let the compiler choose an addressing mode
}

static inline uint64_t rdtsc()
{
    uint64_t ret;
    asm volatile ( "rdtsc" : "=A"(ret) );
    return ret;
}

static inline unsigned long read_cr0()
{
    unsigned long result;
    asm volatile("mov %0, cr0"
            : "=r"(result));
    return result;
}

static inline void write_cr0(unsigned long value)
{
    asm volatile("mov cr0, %0"
            :
            : "a"(value)
            : "memory"
    );
}

static inline unsigned long read_cr2()
{
    unsigned long result;
    asm volatile("mov %0, cr2"
            : "=r"(result));
    return result;
}

static inline void write_cr3(unsigned long value)
{
    asm volatile("mov cr3, %0"
            :
            : "a"(value)
            : "memory"
    );
}

static inline unsigned long read_cr3()
{
    unsigned long result;
    asm volatile("mov %0, cr3"
            : "=r"(result));
    return result;
}

static inline void invlpg(void* m)
{
    /* Clobber memory to avoid optimizer re-ordering access before invlpg, which may cause nasty bugs. */
    asm volatile ( "invlpg [%0]" : : "b"(m) : "memory" );
}

static inline void wrmsr(uint32_t msr_id, uint64_t msr_value)
{
    asm volatile ( "wrmsr" : : "c" (msr_id), "A" (msr_value) );
}

static inline uint64_t rdmsr(uint32_t msr_id)
{
    uint64_t msr_value;
    asm volatile ( "rdmsr" : "=A" (msr_value) : "c" (msr_id) );
    return msr_value;
}

static inline void cli()
{
    asm volatile("cli":::"memory");
}

static inline void sti()
{
    asm volatile("sti":::"memory");
}

static inline void hlt()
{
    asm volatile("hlt":::"memory");
}


