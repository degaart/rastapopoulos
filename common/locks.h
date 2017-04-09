#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "debug.h"
#include "spinlock.h"

typedef uint32_t if_state_t;

if_state_t disable_if();
void restore_if(if_state_t state);

#define enter_critical_section() \
    if_state_t ifstate = disable_if()

#define leave_critical_section() \
    restore_if(ifstate)

/*
 * if `*dest` == `compare`:
 *      `*dest` = `exchange`
 *      return `compare`
 * else:
 *      return `*dest`
 */
extern uint32_t cmpxchg(volatile uint32_t* dest, uint32_t exchange, uint32_t compare);


bool spinlock_try_lock(spinlock_t* lock);
bool spinlock_try_unlock(spinlock_t* lock);

#define checked_lock(lock) \
    do { \
        bool locked = spinlock_try_lock(lock); \
        if(!locked) { \
            panic("Concurrent modification detected on " #lock); \
        } \
    } while(0)

#define checked_unlock(lock) \
    do { \
        bool locked = spinlock_try_unlock(lock); \
        if(!locked) { \
            panic("Concurrent modification detected on " #lock); \
        } \
    } while(0)


extern void cli();
extern void sti();


