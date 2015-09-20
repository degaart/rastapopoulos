#include "io.h"

extern "C" void outb(uint16_t port, uint8_t val);
extern "C" uint8_t inb(uint16_t port);

void IO::outb(uint16_t port, uint8_t val) {
	::outb(port, val);
}

uint8_t IO::inb(uint16_t port) {
	return ::inb(port);
}


