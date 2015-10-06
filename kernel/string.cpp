#include "string.h"

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

void strcpy(char* dst, const char* src) {
    for(; *src; src++)
        *(dst++) = *src;
}

