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

static struct LayoutEntry layout[] = {
    {0x10, 'a'}, {0x30, 'b'}, {0x2e, 'c'}, {0x20, 'd'}, {0x12, 'e'},
    {0x21, 'f'}, {0x22, 'g'}, {0x23, 'h'}, {0x17, 'i'}, {0x24, 'j'},
    {0x25, 'k'}, {0x26, 'l'}, {0x27, 'm'}, {0x31, 'n'}, {0x18, 'o'},
    {0x19, 'p'}, {0x1e, 'q'}, {0x13, 'r'}, {0x1f, 's'}, {0x14, 't'},
    {0x16, 'u'}, {0x2f, 'v'}, {0x2c, 'w'}, {0x2d, 'x'}, {0x15, 'y'},
    {0x11, 'z'}, {0x39, ' '}, {0x09, '_'},
};

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

static int lookup_scancode(uint16_t scancode)
{
    for (size_t i = 0; i < sizeof(layout) / sizeof(layout[0]); i++) {
        if (layout[i].scancode == scancode) {
            return layout[i].ch;
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
    printf("Valid commands:\n");
    for (int i = 0; i < commands_size; i++) {
        printf("%s\n", commands[i].name);
    }
    printf("exec\n");
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

static void exec(char* command, char* arguments)
{
    int ret = sys_exec(command, arguments);
    if (ret == -1) {
        printf("Failed to execute %s\n", command);
    } else {
        printf("%s: %d\n", command, ret);
    }
}

#define X(C) {.name = #C, .fn = C}
static struct Command commands[] = {
    X(help), X(exit), X(exit_thread), X(sleep), X(count), X(shutdown),
};
#undef X
static const size_t commands_size = sizeof(commands) / sizeof(commands[0]);

static void run_command(char* s)
{
    printf("Command: \"");
    for (int i = 0; s[i]; i++) {
        if (s[i] >= ' ') {
            printf("%c", s[i]);
        } else {
            printf("\\x%02x", s[i]);
        }
    }
    printf("\"\n");

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
                }
            }
            if (evt.type == KEY_RELEASED) {
                int ch = 0;
                if (evt.scancode == SCANCODE_LSHIFT ||
                    evt.scancode == SCANCODE_RSHIFT) {
                    shift = 0;
                } else if (evt.scancode == SCANCODE_BACKSPACE) {
                    if (!input_idx)
                        continue;

                    sys_write("\x08", 1);
                    input_idx--;
                } else if (evt.scancode == SCANCODE_ENTER) {
                    sys_write("\n", 1);
                    input_buf[input_idx++] = '\0';
                    run_command(input_buf);
                    break;
                } else {
                    if (input_idx >= 80 - 3)
                        continue;

                    if (shift && evt.scancode >= SCANCODE_1 &&
                        evt.scancode <= SCANCODE_0) {
                        if (evt.scancode == SCANCODE_0)
                            ch = '0';
                        else
                            ch = evt.scancode - SCANCODE_1 + '1';
                        printf("%c", ch);
                        input_buf[input_idx++] = (char)ch;
                        continue;
                    }

                    ch = lookup_scancode(evt.scancode);
                    if (!ch)
                        continue;
                    if (ch >= 'a' && ch <= 'z' && shift) {
                        ch = ch - 'a' + 'A';
                    }
                    printf("%c", ch);
                    input_buf[input_idx++] = (char)ch;
                }
            }
        }
    }
    return 0;
}

