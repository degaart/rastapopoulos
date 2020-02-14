#pragma once

#include <stdint.h>

typedef void(*timer_handler_t)(void*);

void pit_init();
uint64_t pit_get_ticks();
uint64_t pit_get_clock();       /* time since boot in milliseconds */
void* pit_add_timer(uint64_t period, timer_handler_t handler, void* param);      /* period in ms, will return an opaque handle to use in pit_remove_timer() */
void pit_remove_timer(void* handle);

