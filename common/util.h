#pragma once

#include <stdint.h>
#include <stdbool.h>

#define HALT() while(1) { asm volatile("cli\nhlt\n":::"memory"); }
#define ALIGN(P,A) ((((P) + ((A) - 1)) / (A)) * (A))
#define ROUND(P,A) (((P) / (A)) * (A))
#define IS_ALIGNED(P, A)  (((P) & ((A) - 1)) == 0)

#define EFLAGS_IF 0x0200
#define EFLAGS_AC 0x00040000
#define EFLAGS_ID 0x00200000
static inline uint32_t read_eflags(void)
{
    uint32_t result;
    asm volatile("pushf\npop %0"
                 : "=r"(result));
    return result;
}

static void write_eflags(uint32_t eflags)
{
    asm volatile("push %0\npopfd"
                 :
                 : "r"(eflags));
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

#define CR0_PG          (1 << 31)
#define CR0_WP          (1 << 16)
#define CR0_PE          (1 << 0)
static inline uint32_t read_cr0(void)
{
    uint32_t result;
    asm volatile("mov %0, cr0": "=r"(result));
    return result;
}

static inline void write_cr0(uint32_t cr0)
{
    asm volatile("mov cr0, %0" :: "r"(cr0));
}

static inline uint32_t read_cr2(void)
{
    uint32_t result;
    asm volatile("mov %0, cr2": "=r"(result));
    return result;
}

static inline void write_cr2(uint32_t cr2)
{
    asm volatile("mov cr2, %0" :: "r"(cr2));
}

static inline uint32_t read_cr3(void)
{
    uint32_t result;
    asm volatile("mov %0, cr3": "=r"(result));
    return result;
}

static inline void write_cr3(uint32_t cr3)
{
    asm volatile("mov cr3, %0" :: "r"(cr3));
}

#define CLEAR_IF() \
    bool if_enabled = interrupts_enabled(); \
    disable_interrupts()

#define RESTORE_IF() \
    if(if_enabled) \
        enable_interrupts()

bool is_386();
bool is_486();


