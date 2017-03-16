#pragma once

#include <stdint.h>

typedef uint32_t if_state_t;

if_state_t disable_if();
void restore_if(if_state_t state);

/*
 * if `*dest` == `compare`:
 *      `*dest` = `exchange`
 *      return `compare`
 * else:
 *      return `*dest`
 */
extern uint32_t cmpxchg(volatile uint32_t* dest, uint32_t exchange, uint32_t compare);

#define enter_critical_section() \
    if_state_t ifstate = disable_if()

#define leave_critical_section() \
    restore_if(ifstate)


