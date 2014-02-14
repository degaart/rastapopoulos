#include "code16gcc.h"
#include <stdint.h>
#include "bootldr_stub.h"

uint16_t disk_read_chs(
	void* buffer,
	uint16_t device,
	uint16_t c,
	uint16_t h,
	uint16_t s
) {
	struct REG16 regs;
	
	regs.ax = MAKEWORD(1, 0x2);
	regs.cx = MAKEWORD( (s & 0x3f) | ((c & 0x300) >> 2), c);
	regs.dx = MAKEWORD(h, device);
	regs.bx = (uint16_t)buffer;
	_int13(&regs);
	return((regs.ax & 0xFF00) == 0);
}
