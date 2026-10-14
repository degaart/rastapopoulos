#pragma once

#include "gdt.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#define COM1 0x3F8

#define trace(...) _trace(__FILE__, __LINE__, __VA_ARGS__)
#define panic(...) _panic(__FILE__, __LINE__, __VA_ARGS__)
#define assert(cond)                                                          \
    do {                                                                      \
        if (!(cond))                                                          \
            panic("%s", #cond);                                               \
    } while (0)

extern void bootstrap_halt(void) __attribute__((noreturn));

static uint8_t inb(uint16_t port)
{
    uint8_t value;
    asm volatile("in %0, %1" : "=a"(value) : "Nd"(port));
    return value;
}

static void outb(uint16_t port, uint8_t value)
{
    asm volatile("out %1, %0" : : "a"(value), "Nd"(port));
}

static void serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x01); /* 115200 baud */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03); /* 8 data bits, no parity, 1 stop bit */
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x03);
}

static int serial_can_write(void)
{
    return inb(COM1 + 5) & 0x20;
}

static void serial_putchar(char c)
{
    while (!serial_can_write()) {
    }

    outb(COM1, (uint8_t)c);
}

static const char digits[] = "0123456789abcdef";

typedef void (*FormatOutputFn)(void* data, char character);

static int format_unsigned(FormatOutputFn output, void* data,
                           unsigned int value, unsigned int base,
                           unsigned int width)
{
    char buffer[sizeof(unsigned int) * 8];
    unsigned int length = 0;
    int count = 0;

    do {
        buffer[length++] = digits[value % base];
        value /= base;
    } while (value != 0);

    while (width > length) {
        output(data, '0');
        --width;
        ++count;
    }

    while (length != 0) {
        output(data, buffer[--length]);
        ++count;
    }

    return count;
}

static int format_unsigned_long(FormatOutputFn output, void* data,
                                unsigned long value, unsigned int base,
                                unsigned int width)
{
    char buffer[sizeof(unsigned long) * 8];
    unsigned int length = 0;
    int count = 0;

    do {
        buffer[length++] = digits[value % base];
        value /= base;
    } while (value != 0);

    while (width > length) {
        output(data, '0');
        --width;
        ++count;
    }

    while (length != 0) {
        output(data, buffer[--length]);
        ++count;
    }

    return count;
}

static int vformat(FormatOutputFn output, void* data, const char* format,
                   va_list arguments)
{
    int count = 0;

    while (*format != '\0') {
        unsigned int width = 0;
        int long_modifier = 0;

        if (*format != '%') {
            output(data, *format++);
            ++count;
            continue;
        }

        ++format;

        if (*format == '0') {
            ++format;

            while (*format >= '0' && *format <= '9') {
                width = width * 10 + (unsigned int)(*format - '0');
                ++format;
            }
        }

        if (*format == 'l') {
            long_modifier = 1;
            ++format;
        }

        switch (*format) {
        case '%':
            output(data, '%');
            ++count;
            break;

        case 'u':
            if (long_modifier) {
                count += format_unsigned_long(
                    output, data, va_arg(arguments, unsigned long), 10, width);
            } else {
                count += format_unsigned(
                    output, data, va_arg(arguments, unsigned int), 10, width);
            }
            break;

        case 'd':
        case 'i':
        {
            if (long_modifier) {
                long value = va_arg(arguments, long);
                unsigned long magnitude = (unsigned long)value;

                if (value < 0) {
                    output(data, '-');
                    ++count;
                    magnitude = 0ul - magnitude;

                    if (width != 0)
                        --width;
                }

                count +=
                    format_unsigned_long(output, data, magnitude, 10, width);
            } else {
                int value = va_arg(arguments, int);
                unsigned int magnitude = (unsigned int)value;

                if (value < 0) {
                    output(data, '-');
                    ++count;
                    magnitude = 0u - magnitude;

                    if (width != 0)
                        --width;
                }

                count += format_unsigned(output, data, magnitude, 10, width);
            }
            break;
        }

        case 'x':
            if (long_modifier) {
                count += format_unsigned_long(
                    output, data, va_arg(arguments, unsigned long), 16, width);
            } else {
                count += format_unsigned(
                    output, data, va_arg(arguments, unsigned int), 16, width);
            }
            break;

        case 'p':
        {
            void* pointer = va_arg(arguments, void*);

            output(data, '0');
            output(data, 'x');
            count += 2;

            if (width >= 2)
                width -= 2;
            else
                width = 0;

            count += format_unsigned(
                output, data, (unsigned int)(uintptr_t)pointer, 16, width);
            break;
        }

        case 's':
        {
            const char* string = va_arg(arguments, const char*);

            while (*string != '\0') {
                output(data, *string++);
                ++count;
            }

            break;
        }

        case '\0':
            output(data, '%');
            return count + 1;

        default:
            output(data, '%');
            output(data, *format);
            count += 2;
            break;
        }

        ++format;
    }

    return count;
}

static int format(FormatOutputFn output, void* data, const char* fmt, ...)
{
    va_list ap;
    int count;

    va_start(ap, fmt);
    count = vformat(output, data, fmt, ap);
    va_end(ap);

    return count;
}

static void bootstrap_gdt_set_entry(struct GdtEntry* entry, uint32_t base,
                                    uint32_t limit, uint8_t access,
                                    uint8_t flags)
{
    entry->limit_low = limit & 0xffff;
    entry->base_low = base & 0xffff;
    entry->base_mid = (base >> 16) & 0xff;
    entry->access = access;
    entry->limit_high_flags = ((limit >> 16) & 0x0f) | (flags & 0xf0);
    entry->base_high = (base >> 24) & 0xff;
}

static inline void write_cr3(uint32_t value)
{
    asm volatile("mov cr3, %0" : : "r"(value) : "memory");
}

static inline uint32_t read_cr0(void)
{
    uint32_t value;
    asm volatile("mov %0, cr0" : "=r"(value));
    return value;
}

static inline void write_cr0(uint32_t value)
{
    asm volatile("mov cr0, %0" : : "r"(value) : "memory");
}

static inline void* memset(void* dest, int ch, size_t count)
{
    unsigned char* out = dest;

    while (count-- != 0)
        *out++ = (unsigned char)ch;

    return dest;
}

#define VGA_BASE                0xb8000
#define VGA_COLOR_BLACK         0x0
#define VGA_COLOR_BLUE          0x1
#define VGA_COLOR_GREEN         0x2
#define VGA_COLOR_CYAN          0x3
#define VGA_COLOR_RED           0x4
#define VGA_COLOR_MAGENTA       0x5
#define VGA_COLOR_BROWN         0x6
#define VGA_COLOR_LIGHT_GREY    0x7
#define VGA_COLOR_DARK_GREY     0x8
#define VGA_COLOR_LIGHT_BLUE    0x9
#define VGA_COLOR_LIGHT_GREEN   0xA
#define VGA_COLOR_LIGHT_CYAN    0xB
#define VGA_COLOR_LIGHT_RED     0xC
#define VGA_COLOR_LIGHT_MAGENTA 0xD
#define VGA_COLOR_YELLOW        0xE
#define VGA_COLOR_WHITE         0xF

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

#define VGA_CRTC_INDEX 0x3D4
#define VGA_CRTC_DATA  0x3D5

struct VGAPoint
{
    uint8_t x;
    uint8_t y;
};
static volatile uint16_t* vga_buffer = (volatile uint16_t*)VGA_BASE;
static struct VGAPoint cursor;

static inline struct VGAPoint vga_get_cursor_pos(void)
{
    uint16_t pos;

    outb(VGA_CRTC_INDEX, 0x0E);
    pos = (uint16_t)inb(VGA_CRTC_DATA) << 8;

    outb(VGA_CRTC_INDEX, 0x0F);
    pos |= inb(VGA_CRTC_DATA);

    struct VGAPoint result = {.x = pos % 80, .y = pos / 80};
    return result;
}

static inline void vga_set_cursor_pos(struct VGAPoint p)
{
    if (p.x >= VGA_WIDTH)
        p.x = VGA_WIDTH - 1;
    if (p.y >= VGA_HEIGHT)
        p.y = VGA_HEIGHT - 1;
    uint16_t pos = (uint16_t)p.y * 80 + p.x;

    outb(VGA_CRTC_INDEX, 0x0E);
    outb(VGA_CRTC_DATA, (uint8_t)(pos >> 8));

    outb(VGA_CRTC_INDEX, 0x0F);
    outb(VGA_CRTC_DATA, (uint8_t)(pos & 0xFF));
}

static inline void vga_init(void)
{
    cursor = vga_get_cursor_pos();
}

static inline void vga_write(char c, uint8_t fg, uint8_t bg)
{
    const uint16_t attr = (bg << 4) | fg;
    if (c == '\n') {
        cursor.x = 0;
        cursor.y++;
    } else {
        vga_buffer[cursor.y * VGA_WIDTH + cursor.x] = (attr << 8) | (uint8_t)c;

        cursor.x++;

        if (cursor.x >= VGA_WIDTH) {
            cursor.x = 0;
            cursor.y++;
        }
    }

    if (cursor.y >= VGA_HEIGHT) {
        /* Scroll screen up by one row. */
        for (size_t i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); ++i) {
            vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
        }

        /* Clear the last row. */
        for (size_t i = VGA_WIDTH * (VGA_HEIGHT - 1);
             i < VGA_WIDTH * VGA_HEIGHT; ++i) {
            vga_buffer[i] = (attr << 8) | ' ';
        }

        cursor.y = VGA_HEIGHT - 1;
    }

    vga_set_cursor_pos(cursor);
}

static inline void vga_write_at(int x, int y, char ch, uint8_t fg, uint8_t bg)
{
    uint16_t attr = (bg << 4) | fg;
    vga_buffer[y * VGA_WIDTH + x] = (attr << 8) | (uint8_t)ch;
}

static void print_output(void* data, char ch)
{
    vga_write(ch, VGA_COLOR_CYAN, VGA_COLOR_BLACK);
}

static inline void printf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vformat(print_output, NULL, fmt, args);
    va_end(args);
}

static void _trace_output(void* data, char ch)
{
    serial_putchar(ch);
}

static void _trace(const char* file, int line, const char* fmt, ...)
{
    format(_trace_output, NULL, "[%s:%d] ", file, line);

    va_list args;
    va_start(args, fmt);
    vformat(_trace_output, NULL, fmt, args);
    va_end(args);

    serial_putchar('\n');

    va_start(args, fmt);
    vformat(print_output, NULL, fmt, args);
    va_end(args);
    print_output(NULL, '\n');
}

static void _panic(const char* file, int line, const char* fmt, ...)
{
    format(_trace_output, NULL, "[%s:%d] PANIC -- ", file, line);
    printf("[%s:%d] PANIC -- ", file, line);

    va_list args;
    va_start(args, fmt);
    vformat(_trace_output, NULL, fmt, args);
    va_end(args);
    serial_putchar('\n');

    va_start(args, fmt);
    vformat(print_output, NULL, fmt, args);
    va_end(args);
    print_output(NULL, '\n');

    bootstrap_halt();
}

