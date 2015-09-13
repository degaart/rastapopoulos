#define _MEM_C_
#include "mem.h"
#include "util.h"

void pokeb(unsigned seg, unsigned ofs, unsigned char val) {
	asm(
		"push ds\n"
		"mov  ds, [bp + 4]\n"
		"mov  bx, [bp + 6]\n"
		"mov  al, [bp + 8]\n"
		"mov  [bx], al\n"
		"pop  ds"
	);
}

void poke(unsigned seg, unsigned ofs, unsigned val) {
	asm(
		"push ds\n"
		"mov  ds, [bp + 4]\n"
		"mov  bx, [bp + 6]\n"
		"mov  ax, [bp + 8]\n"
		"mov  [bx], ax\n"
		"pop  ds"
	);
}

unsigned char peekb(unsigned seg, unsigned ofs) {
	asm(
		"push ds\n"
		"mov  ds, [bp + 4]\n"
		"mov  bx, [bp + 6]\n"
		"mov  al, [bx]\n"
		"mov  ah, 0\n"
		"pop  ds"
	);
}

unsigned peek(unsigned seg, unsigned ofs) {
	asm(
		"push ds\n"
		"mov  ds, [bp + 4]\n"
		"mov  bx, [bp + 6]\n"
		"mov  ax, [bx]\n"
		"pop  ds"
	);
}

int a20_enabled() {
	/*
		check 2 bytes at 0000:7DFE with FFFF:7E0E
		if different: A20 enabled
		else: rotate the two bytes and compare again
		returns 0 in ax if a20 disabled
	*/
	unsigned val0 = peek(0x0, 0x7DFE);
	unsigned val1 = peek(0xFFFF, 0x7E0E);
	if(val0 != val1)
		return 1;

	unsigned rotated = ((val0 & 0xFF) << 8) | (val0  >> 8); 
	poke(0x0, 0x7DFE, rotated);

	unsigned new_val1 = peek(0xFFFF, 0x7E0E);
	poke(0x0, 0x7DFE, val0);

	return new_val1 == rotated;
}

void enable_a20() {
  /* Check if A20 enabled */
  if(!a20_enabled()) {
    asm(
      "mov ax, 0x2401\n"
      "int 0x15"
    );
  }
}

