#include "format.h"

static int formatUnsigned(FormatOutputFn output, void* data, unsigned int value, unsigned int base)
{
    static const char digits[] = "0123456789abcdef";
    char buffer[sizeof(unsigned int) * 8];
    int length = 0;
    int count = 0;

    do
    {
        buffer[length++] = digits[value % base];
        value /= base;
    } while (value != 0);

    while (length != 0)
    {
        output(data, buffer[--length]);
        ++count;
    }

    return count;
}

int vformat(FormatOutputFn output, void* data, const char* format, va_list arguments)
{
    int count = 0;

    while (*format != '\0')
    {
        if (*format != '%')
        {
            output(data, *format++);
            ++count;
            continue;
        }

        ++format;

        switch (*format)
        {
        case '%':
            output(data, '%');
            ++count;
            break;

        case 'u':
            count += formatUnsigned(output, data, va_arg(arguments, unsigned int), 10);
            break;

        case 'd':
        case 'i':
        {
            int value = va_arg(arguments, int);
            unsigned int magnitude = (unsigned int)value;

            if (value < 0)
            {
                output(data, '-');
                ++count;

                /* This also works for INT_MIN. */
                magnitude = 0u - magnitude;
            }

            count += formatUnsigned(output, data, magnitude, 10);
            break;
        }

        case 'x':
            count += formatUnsigned(output, data, va_arg(arguments, unsigned int), 16);
            break;

        case 'p':
        {
            void* pointer = va_arg(arguments, void*);

            output(data, '0');
            output(data, 'x');
            count += 2;

            count += formatUnsigned(output, data, (unsigned int)pointer, 16);
            break;
        }

        case 's':
        {
            const char* string = va_arg(arguments, const char*);

            while (*string != '\0')
            {
                output(data, *string++);
                ++count;
            }

            break;
        }

        case '\0':
            /* Treat a trailing '%' literally. */
            output(data, '%');
            return count + 1;

        default:
            /* Preserve unsupported conversions literally. */
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

