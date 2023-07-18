#pragma once

#include <stdint.h>

#define IDT_PRESENT        (1 << 7)
#define IDT_DPL0           (0)
#define IDT_DPL1           (1 << 5)
#define IDT_DPL2           (2 << 5)
#define IDT_DPL3           (3 << 5)
#define IDT_TASK_GATE      (5)
#define IDT_TSS_32_AVL     (9)
#define IDT_TSS_32_BUSY    (11)
#define IDT_INT_GATE_32    (14)
#define IDT_TRAP_GATE_32   (15)

struct isr_regs {
    uint32_t ds;                                        // Data segment selector
    uint32_t edi, esi, ebp, unused, ebx, edx, ecx, eax; // Pushed by pusha.
    uint32_t int_no, err_code;                          // Interrupt number and error code (if applicable)
    uint32_t eip, cs, eflags, esp, ss;                  // Pushed by the processor automatically.
    // esp and ss are only populated if the interrupt originated
    // from usermode
} __attribute__((packed));

typedef void (*isr_t)(struct isr_regs*);

void idt_init(void);
void idt_add_handler(int number, isr_t handler, unsigned dpl);

