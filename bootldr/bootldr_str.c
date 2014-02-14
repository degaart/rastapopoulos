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

