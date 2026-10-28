#pragma once

#include "trace.h"
#include <stdint.h>

#define CR0_PE 0x1
#define CR0_PG 0x80000000
#define CR0_WP 0x10000

#define KERNEL_OFFSET 0xc0000000
#define PAGE_SIZE     4096

/* Recursive mappings region of virtual address space (4Mb) */
#define KERNEL_RECURSIVE_START 0xffc00000

/* General mappings region of virtual address space (256Mb) */
#define KERNEL_MAPPINGS_END   KERNEL_RECURSIVE_START
#define KERNEL_MAPPINGS_START 0xf0000000

/* Heap region of virtual address space */
#define KERNEL_HEAP_END KERNEL_MAPPINGS_START

#define EFLAGS_IF (1 << 9)

#define NODISCARD     __attribute__((warn_unused_result))
#define ALIGNED(a)    __attribute__((aligned(a)))
#define UNREACHABLE() __builtin_unreachable()

extern uint8_t __kernel_start[];
extern uint8_t __kernel_end[];
extern uint8_t initial_stack_top[];
extern uint8_t initial_stack[];
extern uint8_t __kernel_phys_start[];
extern uint8_t __bootstrap_start[];
extern uint8_t __bootstrap_end[];
extern uint8_t __kernel_phys_end[];
extern uint8_t __kernel_text_start[];
extern uint8_t __kernel_text_end[];
extern uint8_t __kernel_rodata_start[];
extern uint8_t __kernel_rodata_end[];
extern uint8_t __kernel_data_start[];
extern uint8_t __kernel_data_end[];
extern uint8_t __kernel_init_array_start[];
extern uint8_t __kernel_init_array_end[];
extern uint8_t __kernel_bss_start[];
extern uint8_t __kernel_bss_end[];

/* A must be a power of two */
#define ALIGN_DOWN(V, A) ((__typeof(V))(((uintptr_t)(V)) & ~((A) - 1)))
#define ALIGN_UP(V, A)   ALIGN_DOWN((V) + (A) - 1, A)
#define IS_ALIGNED(value, alignment)                                          \
    (((uintptr_t)(value) & ((uintptr_t)(alignment) - 1)) == 0)

static inline __attribute__((always_inline)) uint8_t inb(uint16_t port)
{
    uint8_t value;
    asm volatile("in %0, %1" : "=a"(value) : "Nd"(port));
    return value;
}

static inline __attribute__((always_inline)) void outb(uint16_t port,
                                                       uint8_t value)
{
    asm volatile("out %1, %0" : : "a"(value), "Nd"(port));
}

static inline __attribute__((always_inline)) void outw(uint16_t port,
                                                       uint16_t value)
{
    asm volatile("out %1, %0" : : "a"(value), "Nd"(port));
}

static inline uint32_t read_cr0(void)
{
    uint32_t value;
    asm volatile("mov %0, cr0" : "=r"(value));
    return value;
}

static inline uint32_t read_cr2(void)
{
    uint32_t value;
    asm volatile("mov %0, cr2" : "=r"(value));
    return value;
}

static inline uint32_t read_cr3(void)
{
    uint32_t value;
    asm volatile("mov %0, cr3" : "=r"(value));
    return value;
}

static inline uint16_t read_ss(void)
{
    uint16_t value;
    asm volatile("mov %0, ss" : "=r"(value));
    return value;
}

static inline uint32_t read_eflags(void)
{
    uint32_t eflags;
    asm volatile("pushf\n\t"
                 "pop %0"
                 : "=r"(eflags)
                 :
                 : "memory");

    return eflags;
}

static inline void write_cr0(uint32_t value)
{
    asm volatile("mov cr0, %0" : : "r"(value) : "memory");
}

static inline void write_cr3(uint32_t value)
{
    asm volatile("mov cr3, %0" : : "r"(value) : "memory");
}

static inline void io_wait(void)
{
    outb(0x80, 0);
}

/* Returns true if interrupts were previously enabled */
static inline bool enable_interrupts(void)
{
    uint32_t eflags;
    asm volatile("pushf\n\t"
                 "pop %0\n\t"
                 "sti"
                 : "=r"(eflags)
                 :
                 : "memory");

    return (eflags & EFLAGS_IF) != 0;
}

/* Returns true if interrupts were previously enabled */
static inline bool disable_interrupts(void)
{
    uint32_t eflags;
    asm volatile("pushf\n\t"
                 "pop %0\n\t"
                 "cli"
                 : "=r"(eflags)
                 :
                 : "memory");

    return (eflags & EFLAGS_IF) != 0;
}

void enter_critical_section(void);
void leave_critical_section(void);

static inline void hlt(void)
{
    asm volatile("hlt" ::: "memory");
}

__attribute__((noreturn)) static inline void debugexit(uint8_t status)
{
    outb(0xf4, status);
    while (1)
        ;
}

#define panic(...) __panic(__FILE__, __LINE__, __VA_ARGS__)

__attribute__((format(printf, 3, 4))) void __panic(const char* file, int line,
                                                   const char* fmt, ...);

extern void halt(void) __attribute__((noreturn));
#define debugbreak() asm volatile("xchg bx, bx" ::: "memory")

extern void ring3_jump(void* ring3_eip, void* ring3_esp, uint32_t ring3_cs,
                       uint32_t ring3_ds) __attribute__((noreturn));

