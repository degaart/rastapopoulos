#ifndef _IO_H_
#define _IO_H_

#include <stdint.h>

class IO {
public:
	static void outb(uint16_t port, uint8_t val);
	static uint8_t inb(uint16_t port);
};

#endif

