#ifndef _TIMER_H_
#define _TIMER_H_

#include <stdint.h>
#include "linked_list.h"
#include "pit.h"

class Timer {
    friend PIT;
public:
    typedef void (*timer_callback_t)(void*);
private:
    const uint32_t _id;
    const timer_callback_t _callback;
    void* const _callback_data;
    const uint32_t _period;
    const bool _recurring;
    uint64_t _last_run;

    static LinkedList<Timer> _timers;
    static uint32_t _ticks;
    static uint32_t _current_id;
    static uint64_t _current_timestamp;

    Timer(uint32_t id, timer_callback_t callback, void* callback_data, uint32_t period, bool recurring);
    static void on_tick();
    static uint32_t next_id();
public:
    static uint32_t schedule(timer_callback_t callback, void* data, uint32_t period, bool recurring = true);
    static void unschedule(uint32_t id);
    static uint64_t current_timestamp();
};

#endif

