#pragma once
#include <stdint.h>

typedef void (*timer_t)(uint64_t, void*);

void pit_add_timer(timer_t handler, void* ctx, unsigned interval);
uint64_t get_ticks(void);
void pit_init(void);
