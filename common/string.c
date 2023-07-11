#include "string.h"

size_t strlen(const char* s)
{
    size_t ret = 0;
    while(*(s++))
        ret++;
    return ret;
}

void* memcpy(void* restrict dst, const void* restrict src, size_t len)
{
    unsigned char* d = dst;
    const unsigned char* s = src;
    while(len--) {
        *d++ = *s++;
    }
    return dst;
}

int memcmp(const void* ptr0, const void* ptr1, size_t len)
{
    const char* p0 = ptr0;
    const char* p1 = ptr1;
    while(len) {
        if(*p0 != *p1) {
            return *p0 - *p1;
        }
        p0++;
        p1++;
        len--;
    }
    return 0;
}

void* memset(void* dst, int ch, size_t len)
{
    unsigned char* ptr = dst;
    while(len--) {
        *ptr++ = ch;
    }
    return dst;
}

void itox(char* buffer, size_t size, unsigned value)
{
    if(!value) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    char tmp[12];
    char* p = tmp;
    while(value) {
        int digit = value % 16;
        *(p++) = digit + (digit < 10 ? '0' : 'A' - 10);
        value /= 16;
    }

    for(--p; p>=tmp && size > 1; size--) {
        *(buffer++) = *(p--);
    }
    *buffer = '\0';
}

void itoa(char* buffer, size_t size, unsigned value)
{
    if(value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    char tmp[9];
    char* p = tmp;
    while(value) {
        *(p++) = (value % 10) + '0';
        value /= 10;
    }

    for(--p; p>=tmp && size > 1; size--) {
        *(buffer++) = *(p--);
    }

    *buffer = '\0';
}

#define WRITESTRING(s) \
    for(const char* ch = s; *ch; ch++) {\
        if(!writefn(*ch, ctx)) \
            return; \
    }

void formatv(bool (*writefn)(char,void*), void* ctx, const char* fmt, va_list args)
{
    while(*fmt) {
        switch(*fmt) {
            case '%':
                switch(*(fmt+1)) {
                    case 's':
                    {
                        const char* s = va_arg(args, const char*);
                        WRITESTRING(s);
                        fmt++;
                        break;
                    }
                    case 'u':
                    case 'd':
                    {
                        unsigned value = va_arg(args, unsigned);
                        char buffer[16];
                        itoa(buffer, sizeof(buffer), value);
                        WRITESTRING(buffer);
                        fmt++;
                        break;
                    }
                    case 'x':
                    case 'X':
                    {
                        unsigned value = va_arg(args, unsigned);
                        char buffer[16];
                        itox(buffer, sizeof(buffer), value);
                        WRITESTRING(buffer);
                        fmt++;
                        break;
                    }
                    case 'p':
                    case 'P':
                    {
                        unsigned value = va_arg(args, unsigned);
                        char buffer[16];
                        itox(buffer, sizeof(buffer), value);
                        int pad = 8 - strlen(buffer);
                        WRITESTRING("0x");
                        for(int i = 0; i < pad; i++) {
                            if(!writefn('0', ctx))
                                return;
                        }
                        WRITESTRING(buffer);
                        fmt++;
                        break;
                    }
                    case '%':
                    {
                        fmt++;
                        if(!writefn('%', ctx))
                            return;
                        break;
                    }
                }
                break;
            case '\0':
                break;
            default:
                if(!writefn(*fmt, ctx))
                    return;
                break;
        }
        fmt++;
    }
}

struct snprintf_ctx {
    size_t size;
    int total_written;
    char* ptr;
};

static bool snprintf_writefn(char ch, void* ctxp)
{
    struct snprintf_ctx* ctx = ctxp;
    if(ctx->size) {
        *ctx->ptr = ch;
        ctx->ptr++;
        ctx->size--;
    }
    ctx->total_written++;
    return true;
}

/*
 * Returns the number of characters that would have been printed if the size were unlimited
 * (not including the final ‘\0’).
 * Returns a negative value if an error occurs.
*/
int snprintf(char* restrict str, size_t size, const char* restrict fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    struct snprintf_ctx ctx;
    ctx.ptr = str;
    ctx.size = size - 1;
    ctx.total_written = 0;
    formatv(snprintf_writefn, &ctx, fmt, args);
    int result = ctx.total_written;
    str[ctx.total_written] = '\0';
    return result;
}

const char* basename(const char* filename)
{
    size_t len = strlen(filename);
    const char* p;
    for(p = filename + len; p > filename && *p != '/'; p--)
        ;
    if(*p == '/')
        p++;
    return p;
}
