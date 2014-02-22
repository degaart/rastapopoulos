#include <stdint.h>
#include "kutil.h"
#include "kstring.h"

/* Copy memory byte by byte, forward direction (so no overlapping possible) */
void memcpy(void* dst, const void* src, unsigned amount) {
	while(amount) {
		*((unsigned char*)dst) = *((const unsigned char*)src);
		src++;
		dst++;
		amount--;
	}
}

void memset(void* dst, int val, unsigned siz) {
	uint8_t* p = (uint8_t*)dst;
	while(siz--) {
		*p = val;
		p++;
	}
}

void bzero(void* dst, unsigned siz) {
	memset(dst, 0, siz);
}

/* Shamelessly stolen from UNIX v7 source */
unsigned atoi(const char* str) {
	int n;
	int f;

	n = 0;
	f = 0;
	for(;;str++) {
		switch(*str) {
		case ' ':
		case '\t':
			continue;
		case '-':
			f++;
		case '+':
			str++;
		}
		break;
	}
	while(*str >= '0' && *str <= '9')
		n = n*10 + *str++ - '0';
	return(f? -n: n);
}

/* This one, we're gonna need to code it ourselves */
void itoa(char* str, uint32_t n) {
	if(!n) {
        *str = '0';
        *(str+1) = '\0';
        return;
    }
    
	char* out = str;
    int leading_zeros = 1;
    static const int divisors[] = {
        1, 10, 100, 1000, 10000, 100000, 1000000,
        10000000, 100000000, 1000000000
    };
	for(int32_t digit=9; digit>=0; digit--) {
		uint32_t divisor=divisors[digit];
        
		ASSERT(divisor != 0);
		uint32_t val = n/divisor;
		ASSERT(val < 10);
        if(!leading_zeros || val) {
            leading_zeros = 0;
            *(out++) = '0'+val;
        }
        n %= divisor;
	}
	*out = '\0';
}

#define HEX_CHAR(d) ( ((d)<10) ? ((d)+'0') : ((d)+('A'-10)) )
void itox(char* str, uint32_t n) {
	for(int digit=0; digit<8; digit++) {
		str[7-digit] = HEX_CHAR( (n & (0xF << 4*digit)) >> (4*digit) );
	}
	str[8] = '\0';
}
