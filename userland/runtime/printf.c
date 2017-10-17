#include <runtime.h>
#include <port.h>
#include <debug.h>
#include <stdarg.h>
#include <string.h>
#include "vga_client.h"

struct callback_params {
    char* buffer;
    size_t size;
    size_t offset;
    int ret;
};

static void callback(int ch, void* cp)
{
    struct callback_params* params = cp;

    if(params->offset < params->size - 1) {
        params->buffer[params->offset] = ch;
        params->offset++;
    } else {
        params->buffer[params->offset] = '\0';
        puts(params->buffer);

        params->buffer[0] = ch;
        params->offset = 1;

    }
    params->ret++;
}

int vprintf(const char *format, va_list vlist)
{
    char buf[256];
    struct callback_params params;
    params.buffer = buf;
    params.size = sizeof(buf);
    params.offset = 0;
    params.ret = 0;

    formatv(callback, &params, format, vlist);

    if(params.offset) {
        buf[params.offset] = '\0';
        puts(buf);
    }

    return params.ret;
}

int printf(const char *format,...)
{
    va_list args;
    va_start(args, format);
    int ret = vprintf(format, args);
    va_end(args);
    return ret;
}




