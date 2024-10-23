#pragma once

#include <stdbool.h>
#include <stdint.h>

#define HLT() asm volatile("hlt\n" ::: "memory")

#define HALT()                                                                 \
    while(1) {                                                                 \
        asm volatile("cli\nhlt\n" ::: "memory");                               \
    }

#define RDTSC() __builtin_ia32_rdtsc()

/* The following helper macros only work on powers of two */
#define ALIGN(X, A)          (((X) + ((typeof(X))(A)-1)) & ~((typeof(X))(A)-1))
#define ROUND(X, A)          ((X) & ~(((typeof(X))(A)-1)))
#define ALIGN_PTR(P, A)      ((typeof(P))ALIGN((uintptr_t)(P), (A)))
#define ROUND_PTR(P, A)      ((typeof(P))ROUND((uintptr_t)(P), (A)))
#define IS_ALIGNED(X, A)     (((X) & ((typeof(X))(A)-1)) == 0)
#define IS_ALIGNED_PTR(X, A) IS_ALIGNED((uintptr_t)(X), (A))

#define EFLAGS_CF   0x00000001
#define EFLAGS_PF   0x00000004
#define EFLAGS_AF   0x00000010
#define EFLAGS_ZF   0x00000040
#define EFLAGS_SF   0x00000080
#define EFLAGS_TF   0x00000100
#define EFLAGS_IF   0x00000200
#define EFLAGS_DF   0x00000400
#define EFLAGS_OF   0x00000800
#define EFLAGS_IOPL 0x00003000
#define EFLAGS_NT   0x00004000
#define EFLAGS_MD   0x00008000
#define EFLAGS_RF   0x00010000
#define EFLAGS_VM   0x00020000
#define EFLAGS_AC   0x00040000
#define EFLAGS_VIF  0x00080000
#define EFLAGS_VIP  0x00100000
#define EFLAGS_ID   0x00200000

static inline uint32_t read_eflags(void)
{
    uint32_t result;
    asm volatile("pushf\npop %0" : "=r"(result));
    return result;
}

static inline void write_eflags(uint32_t eflags)
{
    asm volatile("push %0\npopfd" : : "r"(eflags));
}

static inline bool interrupts_enabled(void)
{
    uint32_t eflags = read_eflags();
    return eflags & EFLAGS_IF;
}

static inline void disable_interrupts(void)
{
    asm volatile("cli");
}

static inline void enable_interrupts(void)
{
    asm volatile("sti");
}

#define CR0_PG (1 << 31)
#define CR0_WP (1 << 16)
#define CR0_PE (1 << 0)
static inline uint32_t read_cr0(void)
{
    uint32_t result;
    asm volatile("mov %0, cr0" : "=r"(result));
    return result;
}

static inline void write_cr0(uint32_t cr0)
{
    asm volatile("mov cr0, %0" ::"r"(cr0));
}

static inline uint32_t read_cr2(void)
{
    uint32_t result;
    asm volatile("mov %0, cr2" : "=r"(result));
    return result;
}

static inline void write_cr2(uint32_t cr2)
{
    asm volatile("mov cr2, %0" ::"r"(cr2));
}

static inline uint32_t read_cr3(void)
{
    uint32_t result;
    asm volatile("mov %0, cr3" : "=r"(result));
    return result;
}

static inline void write_cr3(uint32_t cr3)
{
    asm volatile("mov cr3, %0" ::"r"(cr3));
}

static inline uint32_t read_esp()
{
    uint32_t result;
    asm volatile("mov %0, esp" : "=r"(result));
    return result;
}

static inline uint16_t read_es()
{
    uint16_t result;
    asm volatile("mov %0, es" : "=r"(result));
    return result;
}

#define CLEAR_IF()                                                             \
    bool if_enabled = interrupts_enabled();                                    \
    disable_interrupts()

#define RESTORE_IF()                                                           \
    if(if_enabled)                                                             \
    enable_interrupts()

bool is_386();
bool is_486();
