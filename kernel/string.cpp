#include "string.h"
#include "kmalloc.h"

void String::itoa(char* str, unsigned n) {
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

void String::itox(char* str, unsigned n) {
	/*if(!n) {
		str[0] = '0';
		str[1] = '\0';
		return;
	} else if(n < 10) {
		str[0] = '0' + n;
		str[1] = '\0';
		return;
	} else if(n < 16) {
		str[0] = 'A' + (n - 10);
		str[1] = '\0';
		return;
	} else {*/
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
	/*}*/
}

void String::format(
    String::format_callback callback, 
    void* callback_params,
    const char* fmt,
    ...
) {
    va_list args;

    va_start(args, fmt);
    formatv(callback, callback_params, fmt, args);
    va_end(args);
}

void String::formatv(
    String::format_callback callback, 
    void* callback_params,
    const char* fmt,
    va_list args
) {
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
                while(*p)
                    callback(*(p++), callback_params);

                fmt++;
                break;
            case 's':
                p = va_arg(args, char*);

                while(*p)
                    callback(*(p++), callback_params);

                fmt++;
                break;
            case 'X':
            case 'x':
                val = va_arg(args, unsigned);

                itox(num_buffer, val);
                p = num_buffer;
                while(*p)
                    callback(*(p++), callback_params);

                fmt++;
                break;
            case 'p':
            case 'P':
                val = va_arg(args, unsigned);

                num_buffer[0] = '0';
                num_buffer[1] = 'x';
                itox(num_buffer + 2, val);
                p = num_buffer;
                while(*p)
                    callback(*(p++), callback_params);

                fmt++;
                break;
            default:
                if(*(fmt+1)) {
                    callback(*(fmt+1), callback_params);
                    fmt++;
                }
            } //switch(*(fmt+1))
            break;
        default:
            callback(*fmt, callback_params);
            break;
        } //switch(*fmt)
        fmt++;
    } // while(fmt)
}

void memset(void* buffer, int ch, uint32_t size) {
    uint8_t* ptr = (uint8_t*)buffer;
    for(unsigned i=0; i<size; i++)
        ptr[i] = ch;
}

void bzero(void* buffer, uint32_t size) {
    memset(buffer, 0, size);
}

void memcpy(void* dest, const void* source, size_t size) {
    uint8_t* src = (uint8_t*)source;
    uint8_t* dst = (uint8_t*)dest;

    for(; size; size--)
        *(dst++) = *(src++);
}

int memcmp(const void* p0, const void* p1, size_t size) {
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

/* Stolen from FreeBSD 10 */
unsigned strlcpy(char* dst, const char* src, unsigned siz) {
    char *d = dst;
    const char *s = src;
    unsigned n = siz;

    /* Copy as many bytes as will fit */
    if (n != 0) {
        while (--n != 0) {
            if ((*d++ = *s++) == '\0')
                break;
        }
    }

    /* Not enough room in dst, add NUL and traverse rest of src */
    if (n == 0) {
        if (siz != 0)
            *d = '\0';      /* NUL-terminate dst */
        while (*s++)
            ;
    }

    return(s - src - 1);    /* count does not include NUL */
}

/* Stolen from FreeBSD 10 */
unsigned strlcat(char* dst, const char* src, unsigned siz) {
    char *d = dst;
    const char *s = src;
    unsigned n = siz;
    unsigned dlen;

    /* Find the end of dst and adjust bytes left but don't go past end */
    while (n-- != 0 && *d != '\0')
        d++;
    dlen = d - dst;
    n = siz - dlen;

    if (n == 0)
        return(dlen + strlen(s));
    while (*s != '\0') {
        if (n != 1) {
            *d++ = *s;
            n--;
        }
        s++;
    }
    *d = '\0';

    return(dlen + (s - src));   /* count does not include NUL */
}

char* strdup(const char* str) {
    unsigned len = strlen(str) + 1;
    char* s = (char*)kmalloc(len);
    memcpy(s, str, len);
    return s;
}

size_t strlen(const char* str) {
    size_t len = 0;
    while(*str)
        len++;
    return len;
}

int strcmp(const char* s0, const char* s1) {
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


