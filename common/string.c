#include "string.h"

#ifdef UNIT_TESTS
#include "debug.h"
#endif

size_t strlen(const char* s)
{
    size_t ret = 0;
    while(*(s++))
        ret++;
    return ret;
}

size_t strlcpy(char* restrict dst, const char* restrict src, size_t dstsize)
{
    char* d = dst;
    const char* s = src;
    size_t n = dstsize;

    if(n) {
        while(--n != 0) {
            if((*d++ = *s++) == '\0')
                break;
        }
    }

    if(!n) {
        if(dstsize)
            *d = '\0';
        while(*s++)
            ;
    }

    return s - src - 1;
}

size_t strlcat(char* restrict dst, const char* restrict src, size_t dstsize)
{
    char* d = dst;
    const char* s = src;
    size_t n = dstsize;
    size_t dlen;

    while(n-- && *d)
        d++;
    dlen = d - dst;
    n = dstsize - dlen;

    if(!n)
        return dlen + strlen(s);
    while(*s) {
        if(n != 1) {
            *d++ = *s;
            n--;
        }
        s++;
    }
    *d = '\0';

    return dlen + (s - src);
}

int strcmp(const char* s1, const char* s2)
{
    while(*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
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

    for(--p; p >= tmp && size > 1; size--) {
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

    char tmp[16];
    char* p = tmp;
    while(value) {
        *(p++) = (value % 10) + '0';
        value /= 10;
    }

    for(--p; p >= tmp && size > 1; size--) {
        *(buffer++) = *(p--);
    }

    *buffer = '\0';
}

#define WRITESTRING(s)                                                         \
    for(const char* ch = s; *ch; ch++) {                                       \
        if(!writefn(*ch, ctx))                                                 \
            return;                                                            \
    }

void formatv(bool (*writefn)(char, void*), void* ctx, const char* fmt,
             va_list args)
{
    int padding = 0;
    bool zeropad = false;
    bool left = false;
    while(*fmt) {
        if(*fmt == '%') {
            fmt++;
            bool exitfmt = false;
            while(*fmt && !exitfmt) {
                switch(*fmt) {
                case 's': {
                    const char* s = va_arg(args, const char*);
                    if(padding && !left) {
                        int padcount = padding - strlen(s);
                        for(int i = 0; i < padcount; i++) {
                            if(!writefn(' ', ctx))
                                return;
                        }
                    }
                    WRITESTRING(s);
                    if(padding && left) {
                        int padcount = padding - strlen(s);
                        for(int i = 0; i < padcount; i++) {
                            if(!writefn(' ', ctx))
                                return;
                        }
                    }
                    exitfmt = true;
                    break;
                }
                case 'c': {
                    int ch = va_arg(args, int);
                    if(ch < ' ' || ch >= 128) {
                        ch = ' ';
                    }
                    if(!writefn(ch, ctx))
                        return;
                    exitfmt = true;
                    break;
                }
                case 'u':
                case 'd':
                case 'x':
                case 'X':
                case 'p': {
                    unsigned value = va_arg(args, unsigned);
                    char buffer[16];
                    switch(*fmt) {
                    case 'u':
                    case 'd':
                        itoa(buffer, sizeof(buffer), value);
                        break;
                    case 'x':
                    case 'X':
                        itox(buffer, sizeof(buffer), value);
                        break;
                    case 'p':
                        WRITESTRING("0x");
                        itox(buffer, sizeof(buffer), value);
                        zeropad = true;
                        padding = 8;
                        break;
                    }
                    if(padding) {
                        int padcount = padding - strlen(buffer);
                        int padchar = zeropad ? '0' : ' ';
                        for(int i = 0; i < padcount; i++) {
                            if(!writefn(padchar, ctx))
                                return;
                        }
                    }
                    WRITESTRING(buffer);
                    exitfmt = true;
                    break;
                }
                case '%': {
                    if(!writefn('%', ctx))
                        return;
                    exitfmt = true;
                    break;
                }
                case 'z':
                case 'l': {
                    /* Ignore */
                    break;
                }
                case '-': {
                    left = true;
                    break;
                }
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9': {
                    if(*fmt == '0' && padding == 0) {
                        zeropad = true;
                    }
                    padding = (padding * 10) + *fmt - '0';
                    break;
                }
                }
                fmt++;
            }
        } else {
            if(!writefn(*fmt, ctx))
                return;
            fmt++;
        }
        padding = 0;
        zeropad = false;
        left = false;
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
 * Returns the number of characters that would have been printed if the size
 * were unlimited (not including the final ‘\0’). Returns a negative value if an
 * error occurs.
 */
int snprintf(char* restrict str, size_t size, const char* restrict fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    return vsnprintf(str, size, fmt, args);
}

int vsnprintf(char* restrict str, size_t size, const char* restrict fmt,
              va_list args)
{
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

#ifdef UNIT_TESTS
#define TEST(expected, ...)                                                    \
    do {                                                                       \
        snprintf(buffer, sizeof(buffer), __VA_ARGS__);                         \
        ASSERT(!strcmp(buffer, expected));                                     \
    } while(0)
void test_format()
{
    char buffer[128];

    TEST("       aBCd", "%11s", "aBCd");
    TEST("      aBCd", "%10s", "aBCd");
    TEST("aBCd       ", "%-11s", "aBCd");
    TEST("aBCd      ", "%-10s", "aBCd");
    TEST("   12345678", "%11X", 0x12345678);
    TEST("   12345678", "%11lX", 0x12345678UL);
    TEST("   12345678", "%11lx", 0x12345678UL);
    TEST("   12345678", "%11x", 0x12345678);
    TEST("   12345678", "%11zX", (size_t)0x12345678);
    TEST("   12345678", "%11zx", (size_t)0x12345678);
    TEST("  12345678", "%10X", 0x12345678);
    TEST("  12345678", "%10lX", 0x12345678UL);
    TEST("  12345678", "%10lx", 0x12345678UL);
    TEST("  12345678", "%10x", 0x12345678);
    TEST("  12345678", "%10zX", (size_t)0x12345678);
    TEST("  12345678", "%10zx", (size_t)0x12345678);
    TEST(" 1234567890", "%11d", 1234567890);
    TEST(" 1234567890", "%11ld", 1234567890UL);
    TEST(" 1234567890", "%11lu", 1234567890UL);
    TEST(" 1234567890", "%11u", 1234567890);
    TEST(" 1234567890", "%11zd", (size_t)1234567890);
    TEST(" 1234567890", "%11zu", (size_t)1234567890);
    TEST("00012345678", "%011X", 0x12345678);
    TEST("00012345678", "%011lX", 0x12345678UL);
    TEST("00012345678", "%011lx", 0x12345678UL);
    TEST("00012345678", "%011x", 0x12345678);
    TEST("00012345678", "%011zX", (size_t)0x12345678);
    TEST("00012345678", "%011zx", (size_t)0x12345678);
    TEST("0012345678", "%010X", 0x12345678);
    TEST("0012345678", "%010lX", 0x12345678UL);
    TEST("0012345678", "%010lx", 0x12345678UL);
    TEST("0012345678", "%010x", 0x12345678);
    TEST("0012345678", "%010zX", (size_t)0x12345678);
    TEST("0012345678", "%010zx", (size_t)0x12345678);
    TEST("01234567890", "%011d", 1234567890);
    TEST("01234567890", "%011ld", 1234567890UL);
    TEST("01234567890", "%011lu", 1234567890UL);
    TEST("01234567890", "%011u", 1234567890);
    TEST("01234567890", "%011zd", (size_t)1234567890);
    TEST("01234567890", "%011zu", (size_t)1234567890);
    TEST("0xDEADBEEF", "%p", (void*)0xDEADBEEF);
    TEST("12345678", "%02X", 0x12345678);
    TEST("12345678", "%02lX", 0x12345678UL);
    TEST("12345678", "%02lx", 0x12345678UL);
    TEST("12345678", "%02x", 0x12345678);
    TEST("12345678", "%02zX", (size_t)0x12345678);
    TEST("12345678", "%02zx", (size_t)0x12345678);
    TEST("12345678", "%2X", 0x12345678);
    TEST("12345678", "%2lX", 0x12345678UL);
    TEST("12345678", "%2lx", 0x12345678UL);
    TEST("12345678", "%2x", 0x12345678);
    TEST("12345678", "%2zX", (size_t)0x12345678);
    TEST("12345678", "%2zx", (size_t)0x12345678);
    TEST("12345678", "%X", 0x12345678);
    TEST("12345678", "%lX", 0x12345678UL);
    TEST("12345678", "%lx", 0x12345678UL);
    TEST("12345678", "%x", 0x12345678);
    TEST("12345678", "%zX", (size_t)0x12345678);
    TEST("12345678", "%zx", (size_t)0x12345678);
    TEST("1234567890", "%010d", 1234567890);
    TEST("1234567890", "%010ld", 1234567890UL);
    TEST("1234567890", "%010lu", 1234567890UL);
    TEST("1234567890", "%010u", 1234567890);
    TEST("1234567890", "%010zd", (size_t)1234567890);
    TEST("1234567890", "%010zu", (size_t)1234567890);
    TEST("1234567890", "%02d", 1234567890);
    TEST("1234567890", "%02ld", 1234567890UL);
    TEST("1234567890", "%02lu", 1234567890UL);
    TEST("1234567890", "%02u", 1234567890);
    TEST("1234567890", "%02zd", (size_t)1234567890);
    TEST("1234567890", "%02zu", (size_t)1234567890);
    TEST("1234567890", "%10d", 1234567890);
    TEST("1234567890", "%10ld", 1234567890UL);
    TEST("1234567890", "%10lu", 1234567890UL);
    TEST("1234567890", "%10u", 1234567890);
    TEST("1234567890", "%10zd", (size_t)1234567890);
    TEST("1234567890", "%10zu", (size_t)1234567890);
    TEST("1234567890", "%2d", 1234567890);
    TEST("1234567890", "%2ld", 1234567890UL);
    TEST("1234567890", "%2lu", 1234567890UL);
    TEST("1234567890", "%2u", 1234567890);
    TEST("1234567890", "%2zd", (size_t)1234567890);
    TEST("1234567890", "%2zu", (size_t)1234567890);
    TEST("1234567890", "%d", 1234567890);
    TEST("1234567890", "%ld", 1234567890UL);
    TEST("1234567890", "%lu", 1234567890UL);
    TEST("1234567890", "%u", 1234567890);
    TEST("1234567890", "%zd", (size_t)1234567890);
    TEST("1234567890", "%zu", (size_t)1234567890);
    TEST("aBCd", "%2s", "aBCd");
    TEST("aBCd", "%-2s", "aBCd");
    TEST("aBCd", "%s", "aBCd");
    TEST("a", "%c", 'a');
    TEST("A", "%c", 'A');
    TEST("aBCd", "%c%s%c", 'a', "BC", 'd');

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
    TEST(" % ", " %% ");
#pragma GCC diagnostic pop
}
#endif
