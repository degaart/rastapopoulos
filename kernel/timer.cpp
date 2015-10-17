#include "timer.h"
#include "string.h"
#include "util.h"
#include "debug.h"

LinkedList<Timer> Timer::_timers;
uint32_t Timer::_ticks = 0;
uint32_t Timer::_current_time = 0;
uint32_t Timer::_current_id = 0;

Timer::Timer(uint32_t id, timer_callback_t callback, void* callback_data, uint32_t period, bool recurring):
    _id(id),
    _callback(callback),
    _callback_data(callback_data),
    _period(period),
    _recurring(recurring),
    _last_run(_current_time) {

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
void Timer::on_tick() {
    _ticks++;
    _current_time += TICKS_PER_MS;

    LinkedList<uint32_t> invalidated_timers;
    for(auto timer = _timers.iterator(); timer.valid(); timer.next()) {
        if(_current_time >= timer->_last_run + timer->_period) {
            timer->_callback(timer->_callback_data);
            timer->_last_run = _current_time;
            if(!timer->_recurring) {
                invalidated_timers.push(timer->_id);
            }
        }
    }

    for(auto it = invalidated_timers.iterator(); it.valid(); it.next()) {
        unschedule(*it);
    }
}

uint32_t Timer::next_id() {
    return _current_id++;
}

