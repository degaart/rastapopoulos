; Rastapopoulos
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
global _memcpyl
global _enter_pmode

extern isr0
extern cstart
extern write_string

_start:
	; setup registers
	cli
	mov ax, 0
	mov ds, ax
	mov es, ax
	mov ss, ax
	and esp, 0xFFFF
	
	; install interrupt handlers
	push es
	mov ax, 0
	mov es, ax
	
	mov word [es:0], _isr0
	mov ax, cs
	mov word [es:2], ax
	
	pop es
	sti
	
	; Setup big unreal mode
	call setup_unreal
	
	; Call C startup
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
		retf
		
		
_write_string:
		; write_string for assembly functions
		; ds:si string pointer
		push bx
	.loop:
		mov al, [si]
		
		test al, al
		jz .return
		xor bh, bh
		xor dl, dl
		mov ah, 0x0E
		int 0x10
		
		inc si
		jmp .loop
	.return:
		pop bx
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
		retf

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
		retf

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
		retf

_enable_a20:
		; enable the A20 gate
		mov ax, 0x2401
		int 0x15
		retf

_isr0:
		pusha
		push gs
		push fs
		push ds
		push es
		
		mov ax, 0
		mov ds, ax

		call dword isr0
	.return:
		pop es
		pop ds
		pop fs
		pop gs
		popa
		iret


setup_unreal:
		cli                    ; no interrupts
		push ds                ; save real mode
		
		lgdt [.gdtinfo]         ; load gdt register
		
		mov  eax, cr0          ; switch to pmode by
		or al,1                ; set pmode bit
		mov  cr0, eax
		
		jmp .no_crash          ; tell 386/486 to not crash
		
	.no_crash:
		mov  bx, 0x08          ; select descriptor 1
		mov  ds, bx            ; 8h = 1000b
		
		and al,0xFE            ; back to realmode
		mov  cr0, eax          ; by toggling bit again
		
		pop ds                 ; get back old segment
		sti
		
		ret
	.gdtinfo:
		dw .gdt_end - .gdt - 1   ;last byte in table
		dd .gdt                 ;start of table
 
	.gdt: dd 0,0        ; entry 0 is always unused
	.flatdesc: db 0xff, 0xff, 0, 0, 0, 10010010b, 11001111b, 0
	.gdt_end:

_enter_pmode:
		; Enters pmode and calls a pmode code
		; Params:
		;	EBP+8	GDT pointer (linear address) (DWORD)
		;	EBP+12	GDT size (DWORD)
		;	EBP+16	Pmode entry (DWORD)
		;
		push ebp
		mov ebp, esp
		
		; GDT size
		; Pay attention!
		; We substract 1 to the gdt size in bytes!!!
		mov ax, [ebp+12]				; size
		shl ax, 3						; size * 8
		dec ax							; (size * 8) - 1
		mov [.gdt_desc_size], ax
		
		mov eax, [ebp+8]
		mov [.gdt_desc_offset], eax
		
		; Store kernel entry point in conventional memory
		; so we can access it again in protected mode
		mov eax, [ebp+16]
		mov dword [0x504], eax
		
		cli
		lgdt [.gdt_desc]
		mov eax, cr0
		or al, 1
		mov cr0, eax

		jmp dword 0x08:entry32
		jmp _halt

	.gdt_desc:
	.gdt_desc_size: dw 0
	.gdt_desc_offset: dd 0
	.gdt_desc_end:

global _get_memmap
_get_memmap:
		; Get memory map using int 0x15,0xE820
		; Parameters:
		;	EBP+8	DWORD		Buffer address
		;	EBP+12	DWORD		Buffer size address. Will contain actual length on exit
		;	EBP+16	DWORD		Continuation value address. Will contain new value on exit
		; Returns:
		;	EAX = 0 if failure
		;
		push ebp
		mov ebp, esp
		pushad
	
		mov edx, 0x534D4150			; magic value
		mov eax, [ebp+16]
		mov ebx, [eax]				; *continuation value
		mov eax, [ebp+12]
		mov ecx, [eax]				; *buffer size
		mov edi, [ebp+8]			; buffer address
		mov eax, 0xE820				; function
		int 0x15
		jc .error
		cmp eax, 0x534D4150
		jne .error
		
		mov eax, [ebp+12]
		mov [eax], ecx				; *buffer size = ecx
		mov eax, [ebp+16]
		mov [eax], ebx				; *continuation value
		xor eax, eax
	.return:
		popad
		pop ebp
		ret
	.error:
		mov eax, 1
		jmp short .return

align 64, db 90
entry32:
		; 32-bit entry point
		use32
		
		call dword [0x504]
		jmp halt32
halt32:
		cli
		hlt
		jmp halt32
