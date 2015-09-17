#include <stdint.h>

void outb(uint16_t port, uint8_t val);
uint8_t inb(uint16_t port);

void main() {
	/*uint8_t* vga = (uint8_t*) 0xB8000;
	*vga = '*';*/

	outb(0xE9, 'X');

	while(1)
		;
}

