#pragma once

#include "ringbuf.h"
#include <stdint.h>

#define EVENT_CAPACITY 64

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

RING_BUFFER_DEFINE(KeyEventQueue, struct KeyEvent, EVENT_CAPACITY);

