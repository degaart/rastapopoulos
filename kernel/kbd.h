#pragma once

#include <stdbool.h>

#define KBD_EVENT_PRESSED  0
#define KBD_EVENT_RELEASED 1

#define KBD_MOD_CTRL  (1 << 0)
#define KBD_MOD_ALT   (1 << 1)
#define KBD_MOD_ALTGR (1 << 2)
#define KBD_MOD_SHIFT (1 << 3)
#define KBD_MOD_SUPER (1 << 4)

#define KBD_SC_LSHIFT 0x2A
#define KBD_SC_RSHIFT 0x36
#define KBD_SC_LCTRL  0x1D
#define KBD_SC_RCTRL  0x9D
#define KBD_SC_LALT   0x38
#define KBD_SC_RALT   0xB8
#define KBD_SC_LSUPER 0xDB
#define KBD_SC_RSUPER 0xDC

struct kbd_event {
    unsigned type;
    unsigned scancode;
    unsigned modifiers;
    int ch;
    char unicode_ch[4];
};

bool kbd_read(struct kbd_event* evt);
void kbd_init(void);
