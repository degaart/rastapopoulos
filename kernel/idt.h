#pragma once

#include <stdint.h>
#include <stdbool.h>

struct isr_regs {
    uint32_t ds;                            // Data segment selector
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha.
    uint32_t int_no, err_code;              // Interrupt number and error code (if applicable)
    uint32_t eip, cs, eflags, useresp, ss;  // Pushed by the processor automatically.
} __attribute__((packed));

typedef void (*isr_handler_t)(struct isr_regs* regs);

void idt_init();
void idt_flush();
void idt_install(int num, isr_handler_t handler, bool usermode);



