#include "string.h"
#include "debug.h"

void strreverse(char* buffer, size_t size)
{
    if(size == (size_t)-1)
        size = strlen(buffer);

    int start = 0;
    int end = size - 1;
    while(start < end) {
        int tmp = *(buffer + start);
        *(buffer + start) = *(buffer + end);
        *(buffer + end) = tmp;

        start++;
        end--;
    }
}

/*
 * Convert unsigned 64-bit integer into string
 * Params:
 *  buffer          Output buffer
 *  size            Buffer size in bytes
 *  n               Number to convert
 *  base            Base. 7, 10 or 16
 * Returns:
 *  Number of bytes that would be written if buffer was large enough
 *  I.e. error if result > size
 */
int format_int(char* buffer, size_t size, unsigned long long n, int base)
{
    int ret = 0;

    if(size) {
        *buffer = '\0';
    }

    char* out = buffer;
    while((!ret) || n) {
        if(size) {
            int d = n % base;
            if(d < 10)
                *out = '0' + d;
            else
                *out = 'A' + (d - 10);

            out++;
            size--;
        }

        n /= base;
        ret++;
    }

    if(size) {
        *out = 0;
        strreverse(buffer, -1);
    }
    ret++;

    return ret;
}

/*
 * Convert unsigned 64-bit integer into string
 * Params:
 *  buffer          Output buffer
 *  size            Buffer size in bytes
 *  n               Number to convert
 * Returns:
 *  Number of bytes that would be written if buffer was large enough
 *  I.e. error if result > size
 */
int lltoa(char* buffer, size_t size, unsigned long long n)
{
    int ret = format_int(buffer, size, n, 10);
    return ret;
}

void itoa(char* str, unsigned n)
{
    format_int(str, 15, n, 10);
}

int itox(char* str, unsigned n)
{
    int ret = format_int(str, 9, n, 16); 
    return ret;
}

uint32_t xtoa(const char* str)
{
    const char* p = str;
    
    uint32_t res = 0;
    while(*p && (p < str + 8)) {
        if(*p >= '0' && *p <= '9') {
            res = (res << 4)|(*p - '0');
        } else if(*p >= 'a' && *p <= 'f') {
            res = (res << 4)|(*p - 'a' + 0xA);
        } else if(*p >= 'A' && *p <= 'F') {
            res = (res << 4)|(*p - 'A' + 0xA);
        } else {
            break;
        }
        p++;
    }
    return res;
}

int formatv(format_callback_t callback, 
            void* callback_params,
            const char* fmt,
            va_list args) 
{
    int ret = 0;
    while(*fmt) {
        char num_buffer[64];
        unsigned val;
        unsigned long long llval;
        char* p;
        int l = 0;
        bool breakwhile = false;
        int width = 0;
        int leading_zero = 0;
        size_t len;

        if(*fmt == '%') {
            while(!breakwhile) {
                fmt++;
                switch(*fmt) {
                    case 'd':
                    case 'u':
                        if(l < 2) {
                            val = va_arg(args, unsigned);
                            format_int(num_buffer, sizeof(num_buffer), val, 10);
                        } else {
                            llval = va_arg(args, unsigned long long);
                            format_int(num_buffer, sizeof(num_buffer), llval, 10);
                        }

                        len = strlen(num_buffer);
                        if(leading_zero && len < width) {
                            for(int i = 0; i < width - len; i++) {
                                callback('0', callback_params);
                                ret++;
                            }
                        }

                        p = num_buffer;
                        while(*p) {
                            callback(*(p++), callback_params);
                            ret++;
                        }

                        breakwhile = true;
                        break;
                    case 's':
                        p = va_arg(args, char*);

                        while(*p) {
                            callback(*(p++), callback_params);
                            ret++;
                        }

                        breakwhile = true;
                        break;
                    case 'X':
                    case 'x':
                        if(l < 2) {
                            val = va_arg(args, unsigned);
                            format_int(num_buffer, sizeof(num_buffer), val, 16);
                        } else {
                            llval = va_arg(args, unsigned long long);
                            format_int(num_buffer, sizeof(num_buffer), llval, 16);
                        }

                        len = strlen(num_buffer);
                        if(leading_zero && len < width) {
                            for(int i = 0; i < width - len; i++) {
                                callback('0', callback_params);
                                ret++;
                            }
                        }

                        p = num_buffer;
                        while(*p) {
                            callback(*(p++), callback_params);
                            ret++;
                        }

                        breakwhile = true;
                        break;
                    case 'p':
                    case 'P':
                        strlcpy(num_buffer, "0x", sizeof(num_buffer));

                        if(l < 2) {
                            val = va_arg(args, unsigned);
                            format_int(num_buffer + 2, sizeof(num_buffer) - 2, val, 16);
                        } else {
                            llval = va_arg(args, unsigned long long);
                            format_int(num_buffer + 2, sizeof(num_buffer) - 2, llval, 16);
                        }

                        p = num_buffer;
                        while(*p) {
                            callback(*(p++), callback_params);
                            ret++;
                        }

                        breakwhile = true;
                        break;
                    case 'l':
                        l++;
                        break;
                    case '0':
                        if(!width) {
                            leading_zero++;
                            break;
                        }
                    case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': 
                        if(!width) {
                            width = *fmt - '0';
                        } else {
                            width = (width * 10) + (*fmt - '0');
                        }
                        break;
                    default:
                        if(*fmt) {
                            callback(*fmt, callback_params);
                            ret++;
                        }
                } //switch(*fmt)
            } // while(!breakwhile)
        } else { // *fmt == '%'
            callback(*fmt, callback_params);
            ret++;
        } // *fmt == '%'
        fmt++;
    } // while(fmt)
    return ret;
}

int format(format_callback_t callback, 
           void* callback_params,
           const char* fmt,
           ...) 
{
    va_list args;

    va_start(args, fmt);
    int ret = formatv(callback, callback_params, fmt, args);
    va_end(args);
    return ret;
}

void memset(void* buffer, int ch, uint32_t size)
{
    unsigned char* ptr = (unsigned char*)buffer;
    for(unsigned i=0; i<size; i++)
        ptr[i] = ch;
}

void bzero(void* buffer, uint32_t size)
{
    memset(buffer, 0, size);
}

void memcpy(void* dest, const void* source, size_t size)
{
    // Stolen from xv6
    const char *s;
    char *d;

    s = source;
    d = dest;
    size_t n = size;
    if(s < d && s + n > d){
        s += n;
        d += n;
        while(n-- > 0)
            *--d = *--s;
    } else {
        while(n-- > 0)
            *d++ = *s++;
    }
}


int memcmp(const void* p0, const void* p1, size_t size)
{
    if(!size)
        return 0;

    uint8_t* ptr0 = (uint8_t*)p0;
    uint8_t* ptr1 = (uint8_t*)p1;

    while(size--) {
        if(*ptr0 != *ptr1)
            return *ptr1 - *ptr0;
        ptr0++;
        ptr1++;
    }
    return 0;
}

size_t strlen(const char* str)
{
    size_t len = 0;
    while(*(str++))
        len++;
    return len;
}

void strlcpy(char* dst, const char* src, unsigned siz)
{
    while(siz > 1 && *src) {
        *dst = *src;
        dst++;
        src++;
        siz--;
    }
    *dst = '\0';
}

void strlcat(char* dst, const char* src, unsigned siz)
{
    while(*dst && siz) {
        dst++;
        siz--;
    }

    while(*src && siz > 1) {
        *(dst++) = *(src++);
        siz--;
    }
    *dst = '\0';
}

int strcmp(const char* s0, const char* s1)
{
    while(1) {
        if(!*s0 || !*s0)
            return *s1 - *s0;
        if(*s0 != *s1)
            return *s1 - *s0;
        s1++;
        s0++;
    }
    return 0;
}

typedef struct {
    char* buf;
    int siz;
} snprintf_t;

static void snprintf_callback(int ch, void* param)
{
    snprintf_t* buf = (snprintf_t*)param;
    if(buf->siz) {
        *(buf->buf) = ch;
        buf->buf++;
        buf->siz--;
    }
}

int vsnprintf(char* buffer, size_t size, const char* fmt, va_list args)
{
    snprintf_t buf;
    buf.buf = buffer;
    buf.siz = size - 1;
    int ret = formatv(snprintf_callback, &buf, fmt, args);
    buffer[ret] = '\0';
    return ret + 1;
}

int snprintf(char* buffer, size_t size, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buffer, size, fmt, args);
    va_end(args);
    return ret;
}

int vsncatf(char* buffer, size_t size, const char* fmt, va_list args)
{
    size_t len = strlen(buffer);
    if(len + 1 >= size)
        return 0;
    int ret = vsnprintf(buffer + len, size - len, fmt, args);
    return ret;
}

int sncatf(char* buffer, size_t size, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    return vsncatf(buffer, size, fmt, args);
}

const char* basename(const char* filename)
{
    const char* result = filename;
    for(; *filename; filename++) {
        if(*filename == '/')
            result = filename;
    }
    if(*result == '/')
        result++;
    return result;
}




