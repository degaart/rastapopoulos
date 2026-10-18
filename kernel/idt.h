#pragma once

#include <stdint.h>

#define IDT_TASK_GATE    0x5
#define IDT_INT_GATE_16  0x6
#define IDT_TRAP_GATE_16 0x7
#define IDT_INT_GATE_32  0xe
#define IDT_TRAP_GATE_32 0xf
#define IDT_DPL0 (0 << 5]
#define IDT_DPL1    (1 << 5)
#define IDT_DPL2    (2 << 5)
#define IDT_DPL3    (3 << 5)
#define IDT_PRESENT (1 << 7)

#define INT_DIVIDE_ERROR         0x00
#define INT_DEBUG_EXCEPTION      0x01
#define INT_NMI                  0x02
#define INT_BREAKPOINT           0x03
#define INT_OVERFLOW             0x04
#define INT_BOUND_RANGE          0x05
#define INT_INVALID_OPCODE       0x06
#define INT_DEVICE_UNAVAIL       0x07
#define INT_DOUBLE_FAULT         0x08
#define INT_INVALID_TSS          0x0a
#define INT_SEGMENT_NOT_PRESENT  0x0b
#define INT_STACK_SEGMENT_FAULT  0x0c
#define INT_GPF                  0x0d
#define INT_PF                   0x0e
#define INT_FP_EXCEPTION         0x10
#define INT_ALIGNMENT_CHECK      0x11
#define INT_MACHINE_CHECK        0x12
#define INT_SIMD_FP_EXCEPTION    0x13
#define INT_VIRT_EXCEPTION       0x14
#define INT_CONTROL_PROTECTION   0x15
#define INT_HYPERVISOR_INJECTION 0x1c
#define INT_VMM_COMM_EXCEPTION   0x1d
#define INT_SECURITY_EXCEPTION   0x1e

struct idt_entry
{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t reserved;
    uint8_t flags;
    uint16_t offset_high;
} __attribute__((packed));

struct idtr
{
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct isr_regs
{
    uint32_t ds;

    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;

    uint32_t interrupt_number;
    uint32_t error_code;

    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
    uint32_t user_esp; /* Only when the interrupt originated from ring3 */
    uint32_t user_ss;  /* Only when the interrupt originated from ring3 */
} __attribute__((packed));

typedef void (*idt_handler_t)(int, struct isr_regs*);

void idt_init(void);
idt_handler_t idt_set_handler(int vector, idt_handler_t handler, unsigned dpl);
extern void idt_load(struct idtr* idtr);

