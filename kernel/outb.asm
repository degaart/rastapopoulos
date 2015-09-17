segment .text

; void outb(uint16_t port, uint8_t val);
global outb
outb:
	push	ebp
	mov		ebp, esp
	mov		edx, [ebp+8]
	mov		eax, [ebp+12]
	out 	dx, al
	pop		ebp
	ret

