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

void write_uint32(uint32_t value) {
	write_char('0', 0, 0);
	write_char('x', 0, 0);
	write_char( HEX_CHAR((value & 0xF0000000)>>28), 0, 0 );
	write_char( HEX_CHAR((value & 0x0F000000)>>24), 0, 0 );
	write_char( HEX_CHAR((value & 0x00F00000)>>20), 0, 0 );
	write_char( HEX_CHAR((value & 0x000F0000)>>16), 0, 0 );
	write_char( HEX_CHAR((value & 0x0000F000)>>12), 0, 0 );
	write_char( HEX_CHAR((value & 0x00000F00)>>8), 0, 0 );
	write_char( HEX_CHAR((value & 0x000000F0)>>4), 0, 0 );
	write_char( HEX_CHAR((value & 0x0000000F)), 0, 0 );
}

void dump_mem(const void* buffer, uint32_t siz) {
	const uint8_t *cbuf = (const uint8_t*)buffer;
	while(siz--) {
		uint32_t c = *cbuf;
		write_char( HEX_CHAR( (c & 0xF0) >> 4 ), 0, 0 );
		write_char( HEX_CHAR( c & 0xF ), 0, 0 );
		write_char(' ', 0, 0);
		cbuf++;
	}
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

void memcpy(void* dst, const void* src, uint32_t siz) {
	while(siz--) {
		*((uint8_t*)dst) = *((const uint8_t*)src);
		dst++;
		src++;
	}
}
