#define _UTIL_C_
#include "util.h"
#undef _UTIL_C_

/* This one, we're gonna need to code it ourselves */
void itoa(char* str, unsigned n) {
	if(!n) {
        *str = '0';
        *(str+1) = '\0';
        return;
    } else if(n < 10) {
    	*str = '0' - n;
    	*(str+1) = '\0';
    	return;
    } else {
		char* out = str;

		/* max: 65536 */
		unsigned current_divisor = 10000;
		while(current_divisor) {
			int digit = n / current_divisor;
			if(digit)
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
