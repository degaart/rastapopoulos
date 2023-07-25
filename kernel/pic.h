#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void (*irq_handler_t)(void);

void pic_eoi(unsigned irq);
void pic_mask(unsigned line);
void pic_unmask(unsigned line);
uint16_t pic_irr(void);
uint16_t pic_isr(void);
void pic_set_irq_handler(unsigned irq, irq_handler_t handler);
void pic_init(void);
