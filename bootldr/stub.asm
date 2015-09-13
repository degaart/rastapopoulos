use16

global _start
extern _main

%define breakpoint xchg bx, bx

_start:
	; sp: 0x7bff
	; cs: 0x0000
	; ss: 0x0000
	; ds: 0x0000
	; ip; 0x7d00
	jmp		_main
