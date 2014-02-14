#include "code16gcc.h"
#include <stdint.h>
#include "bootldr_stub.h"
#include "bootldr_str.h"

void write_char(int ch, int page, int color) {
	struct REG16 regs;
	regs.ax = (0x0E<<8)|ch;
	regs.bx = 0x0007;
	_int10(&regs);
}

void write_string(const char* str) {
	while(*str) {
 		write_char(*str, 0, 0x7);
		str++;
	}
}

#define HEX_CHAR(d) ( ((d)<10) ? ((d)+'0') : ((d)+('A'-10)) )
void write_uint16(uint16_t value) {
	write_char('0', 0, 0);
	write_char('x', 0, 0);
	write_char( HEX_CHAR((value & 0xF000)>>12), 0, 0 );
	write_char( HEX_CHAR((value & 0x0F00)>>8), 0, 0 );
	write_char( HEX_CHAR((value & 0x00F0)>>4), 0, 0 );
	write_char( HEX_CHAR((value & 0x000F)), 0, 0 );
}

int memcmp(const void* s0, const void* s1, uint16_t siz) {
	const uint8_t* esi = (const uint8_t*)s0;
	const uint8_t* edi = (const uint8_t*)s1;
	
	while(siz--) {
		if(*edi != *esi)
			return(1);
	}
	return(0);
}

