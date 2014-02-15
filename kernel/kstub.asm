; RastaPopoul 0S 0.09
;
; Assembly stub for kernel
;

bits 32

extern kmain

%define bkpt xchg bx,bx

global _kstart
global _khalt
global _outb
global _inb
global _breakpoint
global _delay

; Entry-point to the kernel
_kstart:
		; setup registers
		cli
		mov ax, 0x10			; data segment selector
		mov ds, ax
		mov ss, ax
		mov es, ax
		mov fs, ax
		mov gs, ax
		mov esp, 0x7FFFF			; 492031 bytes of stack (480kb)

		call kmain
		jmp _khalt

; halt processor
_khalt:
		cli
		hlt
		jmp _khalt
	
; emit byte at port
_outb:
		; esp+8: value to emit (uint32_t)
		; esp+4: port (uint32_t)
		mov al,[esp+8]
		mov dx,[esp+4]
		out dx,al
		ret

; read byte from port
_inb:
		; esp+4: port (uint32_t)
		; returns: uint8_t
		mov dx,[esp+4]
		in al,dx
		and eax,0x000000FF			; just in case
		ret

; breakpoint for bochs
_breakpoint:
		xchg bx,bx
		ret

; Attempt to delay using some tricks
_delay:
		mov ecx, 0x1FFFFF
	.loop:
		mov word [esp+4], 0x3D5
		call _inb
		dec ecx
		cmp ecx,0
		jne .loop
		ret



