#include "lock.h"
#include "registers.h"
#include "debug.h"

void lock_lock(struct lock* lock)
{
    int eflags_if = read_eflags() & EFLAGS_IF;
    cli();
    lock->c++;
    lock->v = eflags_if;
}

void lock_unlock(struct lock* lock)
{
    assert(lock->c > 0);
    lock->c--;
    if(!lock->c) {
        if(lock->v) {
            sti();
        }
    }
}

