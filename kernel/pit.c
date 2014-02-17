#include <stdint.h>
#include "kterm.h"
#include "ports.h"
#include "kutil.h"

#define PIT_CHANNEL0			(0x0<<6)
#define PIT_CHANNEL1			(0x1<<6)
#define PIT_CHANNEL2			(0x2<<6)
#define PIT_READBACK			(0x3<<6)

#define PIT_LATCH_COUNT			(0x0<<4)
#define PIT_ACCESS_LOBYTE		(0x1<<4)
#define PIT_ACCESS_HIBYTE		(0x2<<4)
#define PIT_ACCESS_WORD			(0x3<<4)

#define PIT_MODE(x)				(((x) & (0x7))<<1)

#define PIT_MODE_BCD			0x1

#define PIT_INPUT_CLOCK			1193180

void pit_set_interval(int hz) {
	unsigned divisor = PIT_INPUT_CLOCK / hz;
	
	_outb(
		PORT_PIT_COMMAND,
		PIT_CHANNEL0|PIT_ACCESS_WORD|PIT_MODE(3)
	);
	_outb(PORT_PIT_CHAN0_DATA, LOBYTE(divisor));
	_outb(PORT_PIT_CHAN0_DATA, HIBYTE(divisor));
}

uint32_t pit_read_count() {
	pushf();
	cli();
	
	/* Send latch */
	_outb(PORT_PIT_COMMAND, PIT_CHANNEL0|PIT_LATCH_COUNT);
	
	/* Read low and high-bytes byte */
	uint8_t count_lo = _inb(PORT_PIT_CHAN0_DATA);
	uint8_t count_hi = _inb(PORT_PIT_CHAN0_DATA);
	
	uint32_t count = MAKEWORD(count_lo, count_hi);
	popf();
	
	return(count);
}

void pit_set_reload(uint16_t val) {
	pushf();
	cli();
	
	_outb(PORT_PIT_CHAN0_DATA, LOBYTE(val));
	_outb(PORT_PIT_CHAN0_DATA, HIBYTE(val));
	
	popf();
}


