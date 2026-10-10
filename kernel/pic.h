#pragma once

#include "idt.h"

#define IRQ_TIMER     0
#define IRQ_KEYBOARD  1
#define IRQ_CASCADE   2
#define IRQ_SERIAL2   3
#define IRQ_SERIAL1   4
#define IRQ_PARPORT2  5
#define IRQ_FDC       6
#define IRQ_PARPORT1  7
#define IRQ_CMOSTIMER 8
#define IRQ_FREE1     9
#define IRQ_FREE2     10
#define IRQ_FREE3     11
#define IRQ_AUX       12
#define IRQ_FPU       13
#define IRQ_ATA1      14
#define IRQ_ATA2      15

typedef void (*irq_handler_t)(int, struct isr_regs*);

void pic_init(void);
void pic_eoi(int irq);
void pic_disable(void);
void pic_mask_irq(int irq);
void pic_unmask_irq(int irq);
irq_handler_t pic_set_irq_handler(int num, irq_handler_t handler);
uint16_t pic_get_irr(void); /* which interrupts have been raised */
uint16_t pic_get_isr(void); /* which interrupts are being serviced */

