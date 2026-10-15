#pragma once

#include <stdint.h>

#define CR0_PE 0x1
#define CR0_PG 0x80000000

#define KERNEL_OFFSET 0xc0000000

#define EFLAGS_IF (1 << 9)

extern uint8_t __kernel_start[];
extern uint8_t __kernel_end[];
extern uint8_t initial_stack_top[];
extern uint8_t initial_stack[];
extern uint8_t __kernel_phys_start[];
extern uint8_t __bootstrap_start[];
extern uint8_t __bootstrap_end[];
extern uint8_t __kernel_phys_end[];

/* A must be a power of two */
#define ALIGN_UP(V, A)   (((V) + (A) - 1) & (~((A) - 1)))
#define ALIGN_DOWN(V, A) ((V) & ~((A) - 1))
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

static inline bool enter_critical_section(void)
{
    return disable_interrupts();
}

static inline void leave_critical_section(bool flag)
{
    if (flag)
        enable_interrupts();
}

static inline void hlt(void)
{
    asm volatile("hlt" ::: "memory");
}

#define panic(...) __panic(__FILE__, __LINE__, __VA_ARGS__)

__attribute__((format(printf, 3, 4))) void __panic(const char* file, int line,
                                                   const char* fmt, ...);

extern void halt(void) __attribute__((noreturn));
#define debugbreak() asm volatile("xchg bx, bx" ::: "memory")

extern void ring3_jump(void* ring3_eip, void* ring3_esp, uint32_t ring3_cs,
                       uint32_t ring3_ds) __attribute__((noreturn));

