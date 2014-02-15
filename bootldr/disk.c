#include "code16gcc.h"
#include <stdint.h>
#include "bootldr_stub.h"
#include "bootldr_str.h"

uint16_t disk_read_chs(
	void* buffer,
	uint16_t device,
	uint16_t c,
	uint16_t h,
	uint16_t s
) {
	struct REG16 regs;
	
	/*write_string("disk_read_chs:\r\n");
	DUMP16(device);
	DUMP16(c);
	DUMP16(h);
	DUMP16(s);*/
	
	regs.ax = MAKEWORD(1, 0x2);
	regs.cx = MAKEWORD( (s & 0x3f) | ((c & 0x300) >> 2), c);
	regs.dx = MAKEWORD(device, h);
	regs.bx = (uint16_t)buffer;
	
	/*DUMP16(regs.ax);
	DUMP16(regs.bx);
	DUMP16(regs.cx);
	DUMP16(regs.dx);*/
	
	_int13(&regs);
	return((regs.ax & 0xFF00) == 0);
}
