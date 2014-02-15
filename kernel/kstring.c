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



