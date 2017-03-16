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

bool spinlock_try_lock(spinlock_t* lock)
{
    uint32_t result = cmpxchg(&lock->l, 1, 0);
    return result == 0;
}

bool spinlock_try_unlock(spinlock_t* lock)
{
    uint32_t result = cmpxchg(&lock->l, 0, 1);
    return result == 1;
}





