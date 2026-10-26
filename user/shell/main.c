#include "../../kernel/scancode.h"
#include <limits.h>
#include <rasta.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define BACKSPACE 0x08
#define SHIFT_IN  0x0F

struct LayoutEntry
{
    uint16_t scancode;
    char ch;
};

/*
 * Table of lowercase keys, in groups of two: scancode and associated char
 * In Code page 437, because that's the one the VGA uses
 */
static uint16_t layout[] = {
    0x10, 'a',    0x30, 'b',    0x2e, 'c', 0x20, 'd',    0x12,   'e',
    0x21, 'f',    0x22, 'g',    0x23, 'h', 0x17, 'i',    0x24,   'j',
    0x25, 'k',    0x26, 'l',    0x27, 'm', 0x31, 'n',    0x18,   'o',
    0x19, 'p',    0x1e, 'q',    0x13, 'r', 0x1f, 's',    0x14,   't',
    0x16, 'u',    0x2f, 'v',    0x2c, 'w', 0x2d, 'x',    0x15,   'y',
    0x11, 'z',    0x32, ',',    0x33, ';', 0x34, ':',    0x35,   '!',
    0x28, '\xa3', 0x2b, '*',    0x1a, '^', 0x1b, '$',    0x29,   '\xfd',
    0x02, '&',    0x03, '\x82', 0x04, '"', 0x05, '\'',   0x06,   '(',
    0x07, '-',    0x08, '\x8a', 0x09, '_', 0x0a, '\x87', 0x0b,   '\x85',
    0x0c, ')',    0x0d, '=',    0x56, '<', 0x52, '0',    0x4f,   '1',
    0x50, '2',    0x51, '3',    0x4b, '4', 0x4c, '5',    0x4d,   '6',
    0x47, '7',    0x48, '8',    0x49, '9', 0x53, '.',    0xe035, '/',
    0x37, '*',    0x4a, '-',    0x4e, '+', 0x39, ' '};

/* Keys when shift is held */
static uint16_t layout_shift[] = {
    0x10, 'A',    0x30, 'B',    0x2e,   'C',  0x20, 'D',  0x12,   'E',  0x21,
    'F',  0x22,   'G',  0x23,   'H',    0x17, 'I',  0x24, 'J',    0x25, 'K',
    0x26, 'L',    0x27, 'M',    0x31,   'N',  0x18, 'O',  0x19,   'P',  0x1e,
    'Q',  0x13,   'R',  0x1f,   'S',    0x14, 'T',  0x16, 'U',    0x2f, 'V',
    0x2c, 'W',    0x2d, 'X',    0x15,   'Y',  0x11, 'Z',  0x32,   '?',  0x33,
    '.',  0x34,   '/',  0x35,   '\x15', 0x28, '%',  0x2b, '\xe6', 0x1a, '\xb0',
    0x1b, '\x9c', 0x29, '\xb1', 0x02,   '1',  0x03, '2',  0x04,   '3',  0x05,
    '4',  0x06,   '5',  0x07,   '6',    0x08, '7',  0x09, '8',    0x0a, '9',
    0x0b, '0',    0x0c, '\xf8', 0x0d,   '+',  0x56, '>',  0x52,   '0',  0x4f,
    '1',  0x50,   '2',  0x51,   '3',    0x4b, '4',  0x4c, '5',    0x4d, '6',
    0x47, '7',    0x48, '8',    0x49,   '9',  0x53, '.',  0xe035, '/',  0x37,
    '*',  0x4a,   '-',  0x4e,   '+',    0x39, ' '};

/* Keys when AltGr is held    */
static uint16_t layout_altgr[] = {
    0x10, '\xb3', 0x30, '\xb4', 0x2e, '\xb5', 0x20, '\xb6', 0x12,   '\xb7',
    0x21, '\xb8', 0x22, '\xb9', 0x23, '\xba', 0x17, '\xbb', 0x24,   '\xbc',
    0x25, '\xbd', 0x26, '\xbe', 0x27, '\xbf', 0x31, '\xc0', 0x18,   '\xc1',
    0x19, '\xc2', 0x1e, '\xc3', 0x13, '\xc4', 0x1f, '\xc5', 0x14,   '\xc6',
    0x16, '\xc7', 0x2f, '\xc8', 0x2c, '\xc9', 0x2d, '\xca', 0x15,   '\xcb',
    0x11, '\xcc', 0x32, '\xcd', 0x33, '\xce', 0x34, '\xcf', 0x35,   '\xd0',
    0x28, '\xd1', 0x2b, '\xd2', 0x1a, '\xd3', 0x1b, '\xd4', 0x29,   '\xd5',
    0x02, '\xd6', 0x03, '~',    0x04, '#',    0x05, '{',    0x06,   '[',
    0x07, '|',    0x08, '`',    0x09, '\\',   0x0a, '^',    0x0b,   '@',
    0x0c, ']',    0x0d, '}',    0x56, '\xd7', 0x52, '0',    0x4f,   '1',
    0x50, '2',    0x51, '3',    0x4b, '4',    0x4c, '5',    0x4d,   '6',
    0x47, '7',    0x48, '8',    0x49, '9',    0x53, '.',    0xe035, '/',
    0x37, '*',    0x4a, '-',    0x4e, '+',    0x39, ' '};

size_t parse_args(char* buf, char** argv, size_t len)
{
    if (!buf || !argv || !len) {
        return 0;
    }

    size_t argc = 0;
    char* src = buf;
    char* dst = buf;

    while (*src && argc < len) {
        while (*src == ' ' || *src == '\t') {
            ++src;
        }

        if (!*src) {
            break;
        }

        argv[argc++] = dst;

        int quoted = 0;

        while (*src) {
            if (!quoted && (*src == ' ' || *src == '\t')) {
                break;
            }

            if (*src == '\\') {
                size_t slashes = 0;

                while (*src == '\\') {
                    ++slashes;
                    ++src;
                }

                if (*src == '"') {
                    for (size_t i = 0; i < slashes / 2; ++i) {
                        *dst++ = '\\';
                    }

                    if (slashes & 1) {
                        *dst++ = '"';
                        ++src;
                        continue;
                    }
                } else {
                    while (slashes--) {
                        *dst++ = '\\';
                    }

                    continue;
                }
            }

            if (*src == '"') {
                ++src;

                if (quoted && *src == '"') {
                    *dst++ = '"';
                    ++src;
                } else {
                    quoted = !quoted;
                }

                continue;
            }

            *dst++ = *src++;
        }

        if (*src) {
            ++src;
        }

        *dst++ = '\0';
    }

    return argc;
}

static int lookup_scancode(uint16_t scancode, bool shift, bool altgr)
{
    const uint16_t* table = layout;
    size_t table_len = sizeof(layout) / sizeof(layout[0]);

    if (altgr) {
        table = layout_altgr;
        table_len = sizeof(layout_altgr) / sizeof(layout_altgr[0]);
    } else if (shift) {
        table = layout_shift;
        table_len = sizeof(layout_shift) / sizeof(layout_shift[0]);
    }

    for (size_t i = 0; i < table_len; i += 2) {
        if (table[i] == scancode) {
            return table[i + 1];
        }
    }
    return 0;
}

bool parse_integer(const char* buffer, int* out)
{
    if (!buffer || !out) {
        return false;
    }

    bool negative = false;

    if (*buffer == '-' || *buffer == '+') {
        negative = (*buffer == '-');
        ++buffer;
    }

    if (*buffer < '0' || *buffer > '9') {
        return false;
    }

    unsigned int limit =
        negative ? (unsigned int)INT_MAX + 1 : (unsigned int)INT_MAX;

    unsigned int value = 0;

    while (*buffer >= '0' && *buffer <= '9') {
        unsigned int digit = (unsigned int)(*buffer - '0');

        if (value > (limit - digit) / 10) {
            return false;
        }

        value = value * 10 + digit;
        ++buffer;
    }

    *out = negative ? (int)(0 - value) : (int)value;
    return true;
}

static void split_command_line(char* buf, char** command, char** args)
{
    *command = NULL;
    *args = NULL;

    if (!buf) {
        return;
    }

    char* src = buf;

    while (*src == ' ' || *src == '\t') {
        ++src;
    }

    if (!*src) {
        return;
    }

    *command = src;

    if (*src == '"') {
        ++src;
        *command = src;

        while (*src && *src != '"') {
            ++src;
        }

        char* end = src;

        if (*src == '"') {
            ++src;
        }

        while (*src == ' ' || *src == '\t') {
            ++src;
        }

        if (*src) {
            *args = src;
        }

        *end = '\0';
    } else {
        while (*src && *src != ' ' && *src != '\t') {
            ++src;
        }

        if (*src) {
            *src++ = '\0';

            while (*src == ' ' || *src == '\t') {
                ++src;
            }

            if (*src) {
                *args = src;
            }
        }
    }
}

struct Command
{
    const char* name;
    void (*fn)(char*);
};
static struct Command commands[];
static const size_t commands_size;

static void help(char* arguments)
{
    printf("Internal commands:\n");
    for (int i = 0; i < commands_size; i++) {
        printf("    %s\n", commands[i].name);
    }

    char buffer[256];
    int ret = sys_list(buffer, sizeof(buffer));
    if (ret < 0)
        return;

    printf("External commands:\n");
    for (const char* str = buffer; *str; str += strlen(str) + 1) {
        char* period = strchr(str, '.');
        if (!period)
            continue;
        printf("    %.*s\n", (int)(period - str), str);
    }
}

static void exit(char* arguments)
{
    int status = 0;
    if (*arguments) {
        if (!parse_integer(arguments, &status) || status < 0 || status > 255) {
            printf("Invalid status number: %s\n", arguments);
            return;
        }
    }
    printf("Exiting with status %d\n", status);
    sys_exit(status);
}

static void exit_thread(char* arguments)
{
    sys_exit_thread();
}

static void sleep(char* arguments)
{
    int ticks = 0;
    if (*arguments) {
        if (!parse_integer(arguments, &ticks) || ticks < 0) {
            printf("Invalid ticks count: %s\n", arguments);
            return;
        }
        printf("Sleeping for %d ticks\n", ticks);
        sys_sleep(ticks);
    }
}

static void count(char* arguments)
{
    int end;
    if (!*arguments) {
        printf("Usage: count <number>\n");
        return;
    } else if (!parse_integer(arguments, &end)) {
        printf("Invalid number: %s\n", arguments);
        return;
    }

    for (int i = 0; i <= end; i++) {
        printf("%d ", i);
    }
    printf("\n");
}

static void shutdown(char* arguments)
{
    sys_shutdown();
}

static void test_vga(char* arguments)
{
    printf("Locking screen\n");
    int ret = sys_lock_screen();
    if (!ret) {
        trace("Failed to lock screen");
        return;
    }

    trace("Screen locked");

    uint8_t pixels[32 * 32];
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x)
            pixels[y * 32 + x] = 32 + ((x / 4 + y / 4) % 2) * 180;
    }
    VGABitmap bitmap = sys_vga_load_bitmap(pixels, sizeof(pixels), 32, 32);
    if (!bitmap)
        trace("Failed to load demo bitmap");

    unsigned frame = 0;
    bool running = true;
    while (running) {
        struct KeyEvent evt;
        int overflow;
        while (sys_get_kbd_event(&evt, &overflow, false) == 0) {
            if (evt.type == KEY_PRESSED)
                running = false;
        }
        if (!running)
            break;

        sys_vga_fillrect(0, 0, 319, 239, 0);
        for (int i = 0; i < 16; ++i) {
            int x = (frame * (i % 3 + 1) + i * 23) % 360 - 40;
            int y = (frame + i * 37) % 280 - 40;
            uint8_t color = 16 + (frame / 3 + i * 13) % 239;
            sys_vga_fillrect(x, y, x + 14, y + 14, color);
            sys_vga_rect(x - 4, y - 4, x + 18, y + 18, color + 1);
            sys_vga_oval(319 - x - 40, y, 319 - x, y + 28, color);
            sys_vga_roundrect(x, 239 - y - 30, x + 48, 239 - y,
                              4 + (frame / 4 + i) % 12, color);
            sys_vga_drawline(160, 120, x, y, color);
        }
        if (bitmap) {
            int x = frame % 576;
            int y = frame % 416;
            if (x > 288)
                x = 576 - x;
            if (y > 208)
                y = 416 - y;
            sys_vga_blit(bitmap, 0, 0, 0, x, y, 32, 32);
            sys_vga_blit(0, x, y, 0, 288 - x, 208 - y, 32, 32);
        }
        sys_vga_rect(0, 0, 319, 239, 255);
        sys_vga_swap();
        ++frame;
        sys_sleep(2);
    }
    if (bitmap)
        sys_vga_destroy_bitmap(bitmap);

    trace("Unlocking screen");
    if (!sys_unlock_screen()) {
        trace("Failed to unlock screen");
        return;
    }
    printf("Screen unlocked\n");
}

static void exec(char* command, char* arguments)
{
    int ret = sys_exec(command, arguments);
    if (ret >= 0) {
        printf("%s: %d\n", command, ret);
        return;
    }

    char tmp[81];
    strlcpy(tmp, command, sizeof(tmp));
    strlcat(tmp, ".bin", sizeof(tmp));
    ret = sys_exec(tmp, arguments);
    if (ret >= 0) {
        printf("%d", ret);
        return;
    }
    printf("Failed to execute %s\n", tmp);
}

#define X(C) {.name = #C, .fn = C}
static struct Command commands[] = {
    X(help),  X(exit),     X(exit_thread), X(sleep),
    X(count), X(shutdown), X(test_vga),
};
#undef X
static const size_t commands_size = sizeof(commands) / sizeof(commands[0]);

static void run_command(char* s)
{
    char* command;
    char* arguments;
    split_command_line(s, &command, &arguments);
    if (!arguments)
        arguments = "";

    if (!command || !*command)
        return;

    for (int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        if (!strcmp(commands[i].name, command)) {
            commands[i].fn(arguments);
            return;
        }
    }

    exec(command, arguments);
}

int main(void)
{
    char input_buf[128];
    int input_idx = 0;
    int shift = 0;
    int alt = 0;

    while (true) {
        sys_write("> ", 2);
        input_idx = 0;
        while (true) {
            struct KeyEvent evt;
            int overflow;
            if (sys_get_kbd_event(&evt, &overflow, true) < 0) {
                printf("\nFailed to get KBD event\n");
                return 1;
            }

            if (overflow) {
                shift = 0;
                continue;
            }

            if (evt.type == KEY_PRESSED) {
                if (evt.scancode == SCANCODE_LSHIFT ||
                    evt.scancode == SCANCODE_RSHIFT) {
                    shift = 1;
                } else if (evt.scancode == SCANCODE_LALT ||
                           evt.scancode == SCANCODE_RALT) {
                    alt = 1;
                } else if (evt.scancode == SCANCODE_BACKSPACE) {
                    if (!input_idx)
                        continue;

                    sys_write("\x08", 1);
                    input_idx--;
                } else if (evt.scancode == SCANCODE_ENTER ||
                           evt.scancode == SCANCODE_KP_ENTER) {
                    sys_write("\n", 1);
                    input_buf[input_idx++] = '\0';
                    run_command(input_buf);
                    break;
                } else {
                    if (input_idx >= 80 - 3)
                        continue;

                    int ch = lookup_scancode(evt.scancode, shift, alt);
                    if (!ch) {
                        trace("Failed to translate 0x%x shift: %d alt: %d",
                              evt.scancode, shift, alt);
                        continue;
                    }

                    printf("%c", ch);
                    input_buf[input_idx++] = (char)ch;
                }
            }
            if (evt.type == KEY_RELEASED) {
                int ch = 0;
                if (evt.scancode == SCANCODE_LSHIFT ||
                    evt.scancode == SCANCODE_RSHIFT) {
                    shift = 0;
                } else if (evt.scancode == SCANCODE_LALT ||
                           evt.scancode == SCANCODE_RALT) {
                    alt = 0;
                }
            }
        }
    }
    return 0;
}

