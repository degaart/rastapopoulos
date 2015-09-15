/*
	Rastapopoulos
	I/O functions
*/

#include "io.h"

unsigned inb(unsigned port) {
	asm(
		"xchg bx, bx\n"
		"mov dx, [bp+4]\n"
		"in al, dx\n"
		"and eax, 0x00FF\n"
	);
}

void outb(unsigned port, unsigned byte) {
	asm(
		"mov dx, [bp+4]\n"
		"mov ax, [bp+6]\n"
		"out dx, al\n"
	);
}


void serial_write_char(int ch) {
	outb(0xE9, ch); /* Bochs debug output */

	while(!(inb(0x3fb + 5) & 0x20)); /* wait for queue empty */
	outb(0x3fb, ch);
}

void serial_write_string(const char* str) {
	while(*str)
		serial_write_char(*(str++));

}
