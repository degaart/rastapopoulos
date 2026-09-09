#include "format.h"
#include "rastaldr.h"
#include <stddef.h>

static void writeChar(void* data, char ch)
{
    putc(ch);
}

__attribute__((format(printf, 1, 2)))
int printf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vformat(writeChar, NULL, fmt, args);
    va_end(args);
    return ret;
}

void ldrmain(void)
{
    printf("ldrmain %d%d running\r\n", 420, 69);
    halt();
}


