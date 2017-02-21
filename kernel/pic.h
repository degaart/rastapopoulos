#pragma once

#include "idt.h"

typedef void (*irq_handler_t)(int irq, const struct isr_regs* regs);

#define IRQ_TIMER  0
#define IRQ_KEYBOARD  1
#define IRQ_SERIAL1  4
#define IRQ_SERIAL2  3
#define IRQ_PARPORT2  5
#define IRQ_FDC  6
#define IRQ_PARPORT1  7
#define IRQ_CMOSTIMER  8
#define IRQ_CGARETRACE  9
#define IRQ_AUX  12
#define IRQ_FPU  13
#define IRQ_HDC  14

void pic_init();
void pic_install(int irq, irq_handler_t handler);
void pic_remove(int irq);



