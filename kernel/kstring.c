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
	char* out = str;
	for(int32_t digit=9; digit>=0; digit--) {
		/* poor man's exponentiation */
		uint32_t divisor=1;
		for(uint32_t i=0; i<digit; i++)
			divisor *= 10;
        
		ASSERT(divisor != 0);
		uint32_t val = n/divisor;
		ASSERT(val < 10);
        if(val)
            *(out++) = '0'+val;
		n %= divisor;
	}
	*out = '\0';
}


