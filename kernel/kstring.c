#include <stdint.h>
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

