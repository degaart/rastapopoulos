#include "util.h"

void itoa(char* str, unsigned n) {
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
        unsigned current_divisor = 10000;
        while(current_divisor) {
            int digit = n / current_divisor;
            if(digit || current_divisor == 1)
                *(out++) = '0' + digit;
            
            n %= current_divisor;
            current_divisor /= 10;
        }
        *out = '\0';
    }
}

void itox(char* str, unsigned n) {
	if(!n) {
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
	} else {
		char* out = str;
		unsigned nibble = 4;
        
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
}

void format(write_callback_t callback, void* callback_params, const char* format, ...) {
    formatv(callback, callback_params, format, (char*)&format + sizeof(char*) );
}

void formatv(write_callback_t callback, void* callback_params, const char* format, va_list args) {
    while(*format) {
        char num_buffer[16];
        unsigned val;
        char* p;

        switch(*format) {
        case '%':
            switch(*(format+1)) {
            case 'd':
            case 'u':
                val = *((unsigned*)args);
                args += sizeof(unsigned);

                itoa(num_buffer, val);
                p = num_buffer;
                while(*p)
                    callback(*(p++), callback_params);

                format++;
                break;
            case 's':
                p = *((char**)args);
                args += sizeof(char*);

                while(*p)
                    callback(*(p++), callback_params);

                format++;
                break;
            case 'X':
            case 'x':
                val = *((unsigned*)args);
                args += sizeof(unsigned);

                itox(num_buffer, val);
                p = num_buffer;
                while(*p)
                    callback(*(p++), callback_params);

                format++;
                break;
            case 'p':
            case 'P':
                val = *((unsigned*)args);
                args += sizeof(unsigned);

                num_buffer[0] = '0';
                num_buffer[1] = 'x';
                itox(num_buffer + 2, val);
                p = num_buffer;
                while(*p)
                    callback(*(p++), callback_params);

                format++;
                break;
            default:
                if(*(format+1)) {
                    callback(*(format+1), callback_params);
                    format++;
                }
            } //switch(*(format+1))
            break;
        default:
            callback(*format, callback_params);
            break;
        } //switch(*format)
        format++;
    } // while(format)
}





