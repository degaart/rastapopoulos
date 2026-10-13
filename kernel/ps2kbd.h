#pragma once

#include <stdbool.h>
#include <stdint.h>

enum KeyEventType
{
    KEY_PRESSED,
    KEY_RELEASED,
};

struct KeyEvent
{
    uint16_t scancode;
    enum KeyEventType type;
};

void ps2kbd_init(void);

/* Nonblocking FIFO read. Empty leaves event unchanged */
bool ps2kbd_get_event(struct KeyEvent* event);

/*
 * Returns and clears the lost-event flag. Consumers must reset held-key state
 * after overflow; remaining queued events may be an incomplete history
 */
bool ps2kbd_take_overflow(void);

typedef void (*ps2kbd_observer_t)(void*);
void ps2kbd_add_observer(ps2kbd_observer_t fn, void* arg);
void ps2kbd_remove_observer(ps2kbd_observer_t fn);

