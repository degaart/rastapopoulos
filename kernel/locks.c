#include "locks.h"
#include "registers.h"
#include "kernel.h"

if_state_t disable_if()
{
    unsigned eflags = read_eflags();
    if_state_t result = (eflags & EFLAGS_IF) ? 1 : 0;
    cli();
    return result;
}

void restore_if(if_state_t state)
{
    if(state)
        sti();
    else
        cli();
}


