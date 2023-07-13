#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef void (*irq_handler_t)();

void pic_eoi(unsigned irq);
void pic_mask(unsigned line);
void pic_unmask(unsigned line);
uint16_t pic_irr();
uint16_t pic_isr();
void pic_set_irq_handler(unsigned irq, irq_handler_t handler);
void pic_init();



