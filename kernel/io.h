/*
 * Mainly functions to access I/O ports
 */
#pragma once

#include <stdint.h>

#define IOPORT_DEBUG 0xE9

static inline void outb(uint16_t port, uint8_t value)
{
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

static inline void io_wait()
{
    outb(0x80, 0);
}


