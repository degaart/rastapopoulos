; RastaPopoul 0S 0.08
;
; Assembly stub for kernel
;

bits 32

extern _kmain

global _kstart
global _khalt
global outb
global inb
global breakpoint
global delay

; Entry-point to the kernel
_kstart:
		call _kmain

; halt processor
_khalt:
		cli
		hlt
		jmp _khalt
	
; emit byte at port
outb:
		; esp+8: value to emit (uint32_t)
		; esp+4: port (uint32_t)
		mov al,[esp+8]
		mov dx,[esp+4]
		out dx,al
		ret

; read byte from port
inb:
		; esp+4: port (uint32_t)
		; returns: uint8_t
		mov dx,[esp+4]
		in al,dx
		and eax,0x000000FF			; just in case
		ret

; breakpoint for bochs
breakpoint:
		xchg bx,bx
		ret

; Attempt to delay using some tricks
delay:
		mov ecx, 0x1FFFFF
	.loop:
		mov word [esp+4], 0x3D5
		call inb
		dec ecx
		cmp ecx,0
		jne .loop
		ret
		
		
