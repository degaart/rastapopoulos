#include "kernel.h"
#include "string.h"
#include "debug.h"
#include "kmalloc.h"

void USERFUNC itoa(char* str, unsigned n)
{
    if(n == 0) {
        *str = '0';
        *(str+1) = '\0';
        return;
    } else if(n < 10) {
        *str = '0' + n;
        *(str+1) = '\0';
        return;
    } else {
        char* out = str;
        
        /* max: 65536 */
        unsigned current_divisor = 1000000000;
        bool zeroes = true;
        while(current_divisor) {
            int digit = n / current_divisor;
            if(digit) {
                *(out++) = '0' + digit;
                zeroes = false;
            } else if(!zeroes)
                *(out++) = '0' + digit;

            
            n %= current_divisor;
            current_divisor /= 10;
        }
        *out = '\0';
    }
}

void USERFUNC itox(char* str, unsigned n)
{
		char* out = str;
		unsigned nibble = 8;
        
		while(nibble) {
            unsigned shift = (nibble - 1) * 4;
			int digit = (n >> shift) & 0x0F;
            if(digit < 10)
                *(out++) = (char)('0' + digit);
            else
                *(out++) = (char)('A' + digit - 10);
           
            nibble--;
    	}
        *out = '\0';
}

uint32_t USERFUNC xtoa(const char* str)
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

int USERFUNC formatv(format_callback_t callback, 
                     void* callback_params,
                     const char* fmt,
                     va_list args) 
{
    int ret = 0;
    while(*fmt) {
        char num_buffer[16];
        unsigned val;
        char* p;

        switch(*fmt) {
        case '%':
            switch(*(fmt+1)) {
            case 'd':
            case 'u':
                val = va_arg(args, unsigned);

                itoa(num_buffer, val);
                p = num_buffer;
                while(*p) {
                    callback(*(p++), callback_params);
                    ret++;
                }

                fmt++;
                break;
            case 's':
                p = va_arg(args, char*);

                while(*p) {
                    callback(*(p++), callback_params);
                    ret++;
                }

                fmt++;
                break;
            case 'X':
            case 'x':
                val = va_arg(args, unsigned);

                itox(num_buffer, val);
                p = num_buffer;
                while(*p) {
                    callback(*(p++), callback_params);
                    ret++;
                }

                fmt++;
                break;
            case 'p':
            case 'P':
                val = va_arg(args, unsigned);

                num_buffer[0] = '0';
                num_buffer[1] = 'x';
                itox(num_buffer + 2, val);
                p = num_buffer;
                while(*p) {
                    callback(*(p++), callback_params);
                    ret++;
                }

                fmt++;
                break;
            default:
                if(*(fmt+1)) {
                    callback(*(fmt+1), callback_params);
                    fmt++;
                    ret++;
                }
            } //switch(*(fmt+1))
            break;
        default:
            callback(*fmt, callback_params);
            ret++;
            break;
        } //switch(*fmt)
        fmt++;
    } // while(fmt)
    return ret;
}

int USERFUNC format(format_callback_t callback, 
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

void USERFUNC memset(void* buffer, int ch, uint32_t size)
{
    uint8_t* ptr = (uint8_t*)buffer;
    for(unsigned i=0; i<size; i++)
        ptr[i] = ch;
}

void USERFUNC bzero(void* buffer, uint32_t size)
{
    memset(buffer, 0, size);
}

void USERFUNC memcpy(void* dest, const void* source, size_t size)
{
    uint8_t* src = (uint8_t*)source;
    uint8_t* dst = (uint8_t*)dest;

    while(size--) {
        *(dst++) = *(src++);
    }
}

int USERFUNC memcmp(const void* p0, const void* p1, size_t size)
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

size_t USERFUNC strlen(const char* str)
{
    size_t len = 0;
    while(*(str++))
        len++;
    return len;
}

void USERFUNC strlcpy(char* dst, const char* src, unsigned siz)
{
    while(siz > 1 && *src) {
        *dst = *src;
        dst++;
        src++;
        siz--;
    }
    assert(!(*src));
    *dst = '\0';
}

void USERFUNC strlcat(char* dst, const char* src, unsigned siz)
{
    while(*dst && siz) {
        dst++;
        siz--;
    }

    while(*src && siz > 1) {
        *(dst++) = *(src++);
        siz--;
    }
    assert(!(*src));
    *dst = '\0';
}

int USERFUNC strcmp(const char* s0, const char* s1)
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

static USERFUNC void snprintf_callback(int ch, void* param)
{
    snprintf_t* buf = (snprintf_t*)param;
    if(buf->siz) {
        *(buf->buf) = ch;
        buf->buf++;
        buf->siz++;
    }
}

int USERFUNC vsnprintf(char* buffer, size_t size, const char* fmt, va_list args)
{
    snprintf_t buf;
    buf.buf = buffer;
    buf.siz = size - 1;
    int ret = formatv(snprintf_callback, &buf, fmt, args);
    buffer[ret] = '\0';
    return ret + 1;
}

int USERFUNC snprintf(char* buffer, size_t size, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buffer, size, fmt, args);
    va_end(args);
    return ret;
}

int USERFUNC vsncatf(char* buffer, size_t size, const char* fmt, va_list args)
{
    size_t len = strlen(buffer);
    if(len + 1 >= size)
        return 0;
    int ret = vsnprintf(buffer + len, size - len, fmt, args);
    return ret;
}

int USERFUNC sncatf(char* buffer, size_t size, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    return vsncatf(buffer, size, fmt, args);
}

char* strdup(const char* str)
{
    size_t size = strlen(str);
    char* buffer = kmalloc(size + 1);
    memcpy(buffer, str, size + 1);
    return buffer;
}




