use16
section .text

; unsigned inb(unsigned port);
global inb
inb:
	push	ebp
	mov	ebp, esp
	mov	dx, [ebp+8]
	in 	al, dx
	and	eax, 0x00FF
	pop	ebp
	retf

