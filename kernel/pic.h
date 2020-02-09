#pragma once

#include "idt.h"

typedef void (*irq_handler_t)(int irq, const struct isr_regs* regs);

#define IRQ_TIMER       0
#define IRQ_KEYBOARD    1
#define IRQ_CASCADE     2
#define IRQ_SERIAL2     3
#define IRQ_SERIAL1     4
#define IRQ_PARPORT2    5
#define IRQ_FDC         6
#define IRQ_PARPORT1    7
#define IRQ_CMOSTIMER   8
#define IRQ_FREE1       9
#define IRQ_FREE2       10
#define IRQ_FREE3       11
#define IRQ_AUX         12
#define IRQ_FPU         13
#define IRQ_ATA1        14
#define IRQ_ATA2        15

void pic_init();
void pic_install(int irq, irq_handler_t handler);
void pic_remove(int irq);
void irq_mask(int irq);
void irq_unmask(int irq);


