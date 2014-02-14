; Rastapopoulos 0.09
; Assembly stub for 16-bit bootloader
;
use16

%define breakpoint xchg bx, bx
global _start
global _halt
global _write_char
global _breakpoint
global _int10
global _int13
global _check_a20
global _enable_a20

extern cstart

_start:
	mov ax, 0
	mov ds, ax
	mov es, ax
	mov ss, ax

	and esp, 0xFFFF
	call cstart

_halt:
	cli
	hlt
	jmp _halt

_write_char:
		; write char at current cursor position and advance cursor
		; Params:
		; 	Character to print (byte)
		;	Video page (byte)
		;	Foreground color (byte)
		push ebp
		mov ebp, esp
		push ebx
		
		mov al, [ebp+8]
		mov bh, [ebp+12]
		mov dl, [ebp+16]
		mov ah, 0x0E
		int 0x10
		
		pop ebx
		pop ebp
		ret

_breakpoint:
		breakpoint
		ret

_int10:
		push ebp
		mov ebp, esp
		pushad
		
		mov bx, [bp+8]
		mov bp, bx
		
		mov ax, [bp+0]
		mov bx, [bp+2]
		mov cx, [bp+4]
		mov dx, [bp+6]
		mov si, [bp+8]
		mov di, [bp+10]
		int 0x10
		
		mov [bp+0], ax
		mov [bp+2], bx
		mov [bp+4], cx
		mov [bp+6], dx
		mov [bp+8], si
		mov [bp+10], di

		popad
		pop ebp
		ret

_int13:
		push ebp
		mov ebp, esp
		pushad
		
		mov bx, [bp+8]
		mov bp, bx
		
		mov ax, [bp+0]
		mov bx, [bp+2]
		mov cx, [bp+4]
		mov dx, [bp+6]
		mov si, [bp+8]
		mov di, [bp+10]
		int 0x13
		
		mov [bp+0], ax
		mov [bp+2], bx
		mov [bp+4], cx
		mov [bp+6], dx
		mov [bp+8], si
		mov [bp+10], di

		popad
		pop ebp
		ret

_check_a20:
		; check 2 bytes at 0000:7DFE with FFFF:7E0E
		; if different: A20 enabled
		; else: rotate the two bytes and compare again
		; returns 0 in ax if a20 disabled
		push es
		push ds
		
		mov ax, 0
		mov ds, ax
		mov ax, 0xFFFF
		mov es, ax
		
		mov ax, word [ds:0x7DFE]
		cmp ax, word [es:0x7E0E]
		jne .is_enabled
		
		; so they were equal, we rotate the first
		; and compare again
		not word [ds:0x7DFE]
		mov ax, word [ds:0x7DFE]
		cmp ax, word [es:0x7E0E]
		jne .is_enabled
		
		; well, it was disabled because they were equal
		xor eax, eax
		jmp .return

	.is_enabled:
		mov eax, 1
		
	.return:
		pop ds
		pop es
		ret

_enable_a20:
		; enable the A20 gate
		mov ax, 0x2401
		int 0x15
		ret

nop
nop
nop
nop
