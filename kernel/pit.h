#pragma once

#include <stdint.h>

void pit_init(uint32_t frequency);
uint64_t pit_get_ticks(void);
uint64_t pit_ms_to_ticks(uint32_t milliseconds);
uint64_t pit_ticks_to_ms(uint64_t ticks);

#define PIT_CLOCK_HZ 1193182u
/* Channel 2 wraps every 54.9 ms. IRQ0 samples it; longer critical sections
 * must sample it too to preserve elapsed time. The speaker stays disabled. */
uint64_t pit_perf_clock(void);
