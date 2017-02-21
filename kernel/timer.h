#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "idt.h"

typedef void (*timer_callback_t)(void* data, const struct isr_regs* regs);

void timer_init();
uint32_t timer_schedule(timer_callback_t callback, void* data, uint32_t period, bool recurring);
void timer_unschedule(uint32_t id);
uint64_t timer_timestamp();
uint64_t timer_ticks();

