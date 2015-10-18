#include "timer.h"
#include "string.h"
#include "util.h"
#include "debug.h"

LinkedList<Timer> Timer::_timers;
uint32_t Timer::_ticks = 0;
uint32_t Timer::_current_id = 0;
uint64_t Timer::_current_timestamp = 0;

Timer::Timer(uint32_t id, timer_callback_t callback, void* callback_data, uint32_t period, bool recurring):
    _id(id),
    _callback(callback),
    _callback_data(callback_data),
    _period(period),
    _recurring(recurring),
    _last_run(_current_timestamp) {

}

uint32_t Timer::schedule(timer_callback_t callback, void* data, uint32_t period, bool recurring) {
    uint32_t id = next_id();
    _timers.append(Timer(id, callback , data, period, recurring));
    return id;
}

void Timer::unschedule(uint32_t id) {
    for(auto timer = _timers.iterator(); timer.valid(); timer.next()) {
        if(timer->_id == id) {
            _timers.remove(timer);
            return;
        }
    }
}

static const int TICKS_PER_MS = 1000 / PIT::FREQ;
void Timer::on_tick(const isr_regs_t* regs) {
    _ticks++;
    _current_timestamp += TICKS_PER_MS;
    
    /* Make copy of triggered counters, so timers can be reentrant */
    LinkedList<Timer> triggered_timers;
    LinkedList<uint32_t> invalidated_timers;
    for(auto timer = _timers.iterator(); timer.valid(); timer.next()) {
        if(_current_timestamp >= timer->_last_run + timer->_period) {
            timer->_last_run = _current_timestamp;
            if(!timer->_recurring)
                invalidated_timers.push(timer->_id);

            triggered_timers.append(*timer);        }
    }

    /* Remove triggered counters from list */
    for(auto it = invalidated_timers.iterator(); it.valid(); it.next()) {
        unschedule(*it);
    }

    /* Call handlers */
    for(auto timer = triggered_timers.iterator(); timer.valid(); timer.next()) {
        timer->_callback(timer->_callback_data, regs);
    }
}

uint32_t Timer::next_id() {
    return _current_id++;
}

uint64_t Timer::current_timestamp() {
    return _current_timestamp;
}
