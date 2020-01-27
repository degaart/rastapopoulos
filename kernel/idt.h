#pragma once

#include <stdint.h>
#include <stdbool.h>

struct isr_regs {
    uint32_t ds;                                        // Data segment selector
    uint32_t edi, esi, ebp, unused, ebx, edx, ecx, eax; // Pushed by pusha.
    uint32_t int_no, err_code;                          // Interrupt number and error code (if applicable)
    uint32_t eip, cs, eflags, esp, ss;                  // Pushed by the processor automatically.
    // esp and ss are only populated if the interrupt originated
    // from usedmode
} __attribute__((packed));

typedef void(*isr_handler_t)(const struct isr_regs*);

void idt_init();
void idt_install(
        int interrupt,
        isr_handler_t handler,
        bool usermode /* callable from usermode */);

