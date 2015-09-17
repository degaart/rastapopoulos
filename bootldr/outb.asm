use16
segment .text

;void outb(unsigned port, unsigned byte)
global outb
outb:
	push	ebp
	mov		ebp, esp
	mov		edx, [ebp+8]
	mov		eax, [ebp+12]
	out 	dx, al
	pop		ebp
	retf
