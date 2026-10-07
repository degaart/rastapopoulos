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

} __attribute__((packed));

void idt_init(void);

void idt_set_gate(uint8_t interrupt, uint32_t handler_address,
                  uint16_t selector, uint8_t flags);

void isr_handler(struct isr_regs* regs);
extern void idt_load(struct idtr* idtr);
extern void isr0(void);

