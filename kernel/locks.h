#pragma once

#include <stdint.h>

typedef uint32_t if_state_t;

if_state_t disable_if();
void restore_if(if_state_t state);

#define enter_critical_section() \
    if_state_t ifstate = disable_if()

#define leave_critical_section() \
    restore_if(ifstate)


