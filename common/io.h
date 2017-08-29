#pragma once

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t value)
{
    /*
     * Generated code:
     * mod edx, [ebp-4]
     * mov al, [ebp-8]
     * out dx, al
     */
	asm volatile("out %0,%1" : : "dN"(port), "a" (value));
}

static inline uint8_t inb(uint16_t port)
{
	uint8_t value;
	asm volatile("in %0,%1" : "=a" (value) : "dN" (port));
	return value;
}

static inline void outw(uint16_t port, uint16_t value)
{
	asm volatile("out %0,%1" : : "dN" (port), "a" (value));
}

static inline uint16_t inw(uint16_t port)
{
	uint16_t value;
	asm volatile("in %0,%1" : "=a" (value) : "dN" (port));
	return value;
}

static inline void outl(uint16_t port, uint32_t value)
{
	asm volatile("out %0,%1" : : "dN" (port), "a" (value));
}

static inline uint32_t inl(uint16_t port)
{
	uint32_t value;
	asm volatile("in %0,%1" : "=a" (value) : "dN" (port));
	return value;
}

// wait for ~1ms by writing to a dummy port
static void io_delay()
{
    outb(0x80, 0xFF);
}


/* お前はもう死んでいる */


