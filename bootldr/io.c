/*
	Rastapopoulos
	I/O functions
*/

#include "io.h"

void serial_write_char(int ch) {
	outb(0xE9, ch); /* Bochs debug output */

	while(!(inb(0x3fb + 5) & 0x20)); /* wait for queue empty */
	outb(0x3fb, ch);
}

void serial_write_string(const char* str) {
	while(*str)
		serial_write_char(*(str++));

}
