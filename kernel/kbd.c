#include "kbd.h"
#include "kmalloc.h"
#include "obj/keymap_macfr.h"
#include "pic.h"
#include <debug.h>
#include <io.h>
#include <rbuf.h>
#include <stddef.h>
#include <string.h>

#define REG_DATA    0x60
#define REG_CONTROL 0x64
#define EXTENDED    0xE0

#define STATE_DONE  1
#define STATE_WAIT1 1
#define STATE_WAIT5 2

RBUF_DECLARE(kbd_event_buffer, struct kbd_event);

static struct kbd_event events_storage[17];
static kbd_event_buffer_t events_buffer;
static unsigned state;
static uint8_t keystates[255];
static uint8_t readbuf[16];
static size_t readidx = 0;
static unsigned waitfor = 0;

struct keymap_node {
    struct keymap_node* next;
    unsigned scancode;
    char repr[4][4];
};
#define KEYMAP_BUCKET_COUNT 32
static struct keymap_node* keymap[KEYMAP_BUCKET_COUNT];

static bool lookup_key(char* buffer, size_t buffer_len, unsigned scancode,
                       unsigned modifiers)
{
    if(buffer_len < 4)
        return false;

    size_t bucket = scancode % KEYMAP_BUCKET_COUNT;
    for(const struct keymap_node* node = keymap[bucket]; node;
        node = node->next) {
        if(node->scancode == scancode) {
            size_t repr_idx;
            if((modifiers & KBD_MOD_SHIFT) &&
               ((modifiers & KBD_MOD_ALT) || (modifiers & KBD_MOD_ALTGR))) {
                repr_idx = 3;
            } else if((modifiers & KBD_MOD_ALT) ||
                      (modifiers & KBD_MOD_ALTGR)) {
                repr_idx = 2;
            } else if(modifiers & KBD_MOD_SHIFT) {
                repr_idx = 1;
            } else {
                repr_idx = 0;
            }

            if(UTF8_IS4(node->repr[repr_idx][0])) {
                memcpy(buffer, node->repr[repr_idx], 4);
            } else if(UTF8_IS3(node->repr[repr_idx][0])) {
                memcpy(buffer, node->repr[repr_idx], 3);
            } else if(UTF8_IS2(node->repr[repr_idx][0])) {
                memcpy(buffer, node->repr[repr_idx], 2);
            } else if(UTF8_IS1(node->repr[repr_idx][0])) {
                memcpy(buffer, node->repr[repr_idx], 1);
            }
            return true;
        }
    }
    return false;
}

bool kbd_read(struct kbd_event* evt)
{
    if(RBUF_EMPTY(&events_buffer))
        return false;
    struct kbd_event* ret = RBUF_TRY_POP(&events_buffer, evt);
    assert(ret != NULL);
    return true;
}

static unsigned get_mods()
{
    unsigned modifiers = 0;
    if(keystates[KBD_SC_LSHIFT] || keystates[KBD_SC_RSHIFT])
        modifiers |= KBD_MOD_SHIFT;
    if(keystates[KBD_SC_LCTRL] || keystates[KBD_SC_RCTRL])
        modifiers |= KBD_MOD_CTRL;
    if(keystates[KBD_SC_LALT])
        modifiers |= KBD_MOD_ALT;
    if(keystates[KBD_SC_RALT])
        modifiers |= KBD_MOD_ALTGR;
    if(keystates[KBD_SC_LSUPER] || keystates[KBD_SC_RSUPER])
        modifiers |= KBD_MOD_SUPER;
    return modifiers;
}

static void kbd_pressed(uint8_t scancode)
{
    if(!keystates[scancode]) {
        keystates[scancode] = 1;

        struct kbd_event evt;
        evt.type = KBD_EVENT_PRESSED;
        evt.scancode = scancode;
        evt.modifiers = get_mods();
        if(lookup_key(evt.unicode_ch, sizeof(evt.unicode_ch), evt.scancode,
                      evt.modifiers)) {
            if(UTF8_IS1(evt.unicode_ch[0]))
                evt.ch = evt.unicode_ch[0];
            else
                evt.ch = 0;
        } else {
            memset(evt.unicode_ch, 0, sizeof(evt.unicode_ch));
            evt.ch = 0;
        }
        RBUF_PUSH(&events_buffer, evt);
        /*TRACE("kbd: pressed 0x%X", (int)scancode);*/
    }
}

static void kbd_released(uint8_t scancode)
{
    keystates[scancode] = 0;

    struct kbd_event evt;
    evt.type = KBD_EVENT_RELEASED;
    evt.scancode = scancode;
    evt.modifiers = get_mods();
    if(lookup_key(evt.unicode_ch, sizeof(evt.unicode_ch), evt.scancode,
                  evt.modifiers)) {
        if(UTF8_IS1(evt.unicode_ch[0]))
            evt.ch = evt.unicode_ch[0];
        else
            evt.ch = 0;
    } else {
        memset(evt.unicode_ch, 0, sizeof(evt.unicode_ch));
        evt.ch = 0;
    }
    RBUF_PUSH(&events_buffer, evt);
    /*TRACE("kbd: released 0x%X", (int)scancode);*/
}

static void irq_handler()
{
    uint8_t scancode = inb(REG_DATA);
    
    switch(state) {
    case 0:
        if(scancode == 0xE0) { /* 2-byte scancode */
            state = 1;
        } else if(scancode == 0xE1) { /* 3-byte scancode */
            state = 2;
        } else { /* standalone */
            if(scancode & 0x80) {
                kbd_released(scancode & ~0x80);
            } else {
                kbd_pressed(scancode);
            }
            readidx = 0;
            state = 0;
        }
        break;
    case 1:
        if(scancode & 0x80) {
            kbd_released(scancode);
        } else {
            kbd_pressed(scancode | 0x80);
        }
        readidx = 0;
        state = 0;
        break;
    case 2:
        readbuf[readidx++] = scancode;
        if(readidx == 2) {
            if(scancode & 0x80) {
                readbuf[0] &= ~0x80;
                readbuf[1] &= ~0x80;
            }

            if(readbuf[0] == 0x1D && readbuf[1] == 0x45) {
                if(scancode & 0x80) {
                    kbd_released(0x81);
                } else {
                    kbd_pressed(0x81);
                }
            } else {
                TRACE("Unknown scancode: 0xE1 0x%02X 0x%02X", readbuf[0],
                      readbuf[1]);
            }
            state = 0;
            readidx = 0;
            state = 0;
        }
        break;
    }
}

void kbd_init()
{
    pic_set_irq_handler(1, irq_handler);
    RBUF_INIT(&events_buffer, events_storage,
              sizeof(events_storage) / sizeof(events_storage[0]));

    size_t keymap_size = *((uint32_t*)keymap_macfr);
    unsigned char* keymap_ptr = keymap_macfr + sizeof(uint32_t);
    for(size_t i = 0; i < keymap_size; i++) {

        struct keymap_node* node = kmalloc(sizeof(struct keymap_node));
        node->scancode = *keymap_ptr;
        keymap_ptr++;

        size_t bucket = node->scancode % KEYMAP_BUCKET_COUNT;
        node->next = keymap[bucket];
        keymap[bucket] = node;

        for(size_t j = 0; j < 4; j++) {
            if(UTF8_IS4(*keymap_ptr)) {
                assert(UTF8_ISCONT(keymap_ptr[1]));
                assert(UTF8_ISCONT(keymap_ptr[2]));
                assert(UTF8_ISCONT(keymap_ptr[3]));
                node->repr[j][0] = keymap_ptr[0];
                node->repr[j][1] = keymap_ptr[1];
                node->repr[j][2] = keymap_ptr[2];
                node->repr[j][3] = keymap_ptr[3];
                keymap_ptr += 4;
            } else if(UTF8_IS3(*keymap_ptr)) {
                assert(UTF8_ISCONT(keymap_ptr[1]));
                assert(UTF8_ISCONT(keymap_ptr[2]));
                node->repr[j][0] = keymap_ptr[0];
                node->repr[j][1] = keymap_ptr[1];
                node->repr[j][2] = keymap_ptr[2];
                node->repr[j][3] = 0;
                keymap_ptr += 3;
            } else if(UTF8_IS2(*keymap_ptr)) {
                assert(UTF8_ISCONT(keymap_ptr[1]));
                node->repr[j][0] = keymap_ptr[0];
                node->repr[j][1] = keymap_ptr[1];
                node->repr[j][2] = 0;
                node->repr[j][3] = 0;
                keymap_ptr += 2;
            } else if(UTF8_IS1(*keymap_ptr)) {
                node->repr[j][0] = keymap_ptr[0];
                node->repr[j][1] = 0;
                node->repr[j][2] = 0;
                node->repr[j][3] = 0;
                keymap_ptr += 1;
            } else {
                PANIC("Invalid utf8 byte found in keymap");
            }
        }
    }

    // char lookupbuf[4];
    // bool ret = lookup_key(lookupbuf, sizeof(lookupbuf), 0x02, 0);
    // assert(ret == true);
    // assert(lookupbuf[0] == '&');

    // ret = lookup_key(lookupbuf, sizeof(lookupbuf), 0x10, 0);
    // assert(ret == true);
    // assert(lookupbuf[0] == 'a');

    // ret = lookup_key(lookupbuf, sizeof(lookupbuf), 0x10, KBD_MOD_SHIFT);
    // assert(ret == true);
    // assert(lookupbuf[0] == 'A');

    // ret = lookup_key(lookupbuf, sizeof(lookupbuf), 0x31, KBD_MOD_ALT);
    // assert(ret == true);
    // assert(lookupbuf[0] == '~');

    // ret = lookup_key(lookupbuf, sizeof(lookupbuf), 0x34,
    //                  KBD_MOD_ALT | KBD_MOD_SHIFT);
    // assert(ret == true);
    // assert(lookupbuf[0] == '\\');
}
