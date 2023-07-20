#include "user.h"
#include <debug.h>
#include <util.h>

__attribute__((section(".entry"))) void _start()
{
    int result = add(2, 3, 4);
    ASSERT(result == 2 + 3 + 4);

    if(interrupts_enabled())
        TRACE("Interrupts are enabled");
    else
        TRACE("Interrupts are disabled");

    uint32_t counter = 0;
    uint32_t old_ticks = 0xFFFFFFFF;
    while(1) {
        uint32_t ticks = get_ticks();
        if((ticks % 10) == 0 && ticks != old_ticks) {
            TRACE("counter: %lu, ticks: %lu", counter, ticks);
            old_ticks = ticks;
        }
        counter++;
    }
    halt();
}

