#define _TERM_C_
#include "term.h"
#include "util.h"

void write_char(unsigned ch) {
	/*
		First param in 16-bit mode: bp+4
		               32-bit mode: bp+8
	*/
    	asm(
		"push bx\n"
		"mov ah, 0x0E\n"
		"mov al, [bp+4]\n"
		"xor bx, bx\n"
		"int 0x10\n"
	);
}

void write_string(const char* str) {
	while(*str) {
		if(*str == '\n')
			write_char('\r');
		write_char(*str);
		str++;
	}
}

void write_int(unsigned value) {
	char buffer[11];
	itoa(buffer, value);
	write_string(buffer);
}

