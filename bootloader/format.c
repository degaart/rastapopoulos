#include "format.h"
#include <stdint.h>

static const char digits[] = "0123456789abcdef";

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

int vformat(FormatOutputFn output, void* data, const char* format,
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

int format(FormatOutputFn output, void* data, const char* fmt, ...)
{
    va_list ap;
    int count;

    va_start(ap, fmt);
    count = vformat(output, data, fmt, ap);
    va_end(ap);

    return count;
}
