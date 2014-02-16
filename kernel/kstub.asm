; RastaPopoul 0S 0.09
;
; Assembly stub for kernel
;
bits 32

extern kmain

%define breakpoint		xchg bx,bx
%define KERN_DATA_SEL	0x10			; kernel data selector
%define KERN_CODE_SEL   0x08			; kernel code selector
%define VGA_BASE		0xB8000			; VGA base address

; Entry-point to the kernel
global _kstart
_kstart:
		; setup registers
		cli
		mov ax, KERN_DATA_SEL		; data segment selector
		mov ds, ax
		mov ss, ax
		mov es, ax
		mov fs, ax
		mov gs, ax
		mov esp, 0x7FFFF			; 492031 bytes of stack (480kb)

		call kmain
		jmp _khalt

; halt processor
global _halt
_khalt:
		cli
		hlt
		jmp _khalt
	
; emit byte at port
global _outb
_outb:
		; esp+8: value to emit (uint32_t)
		; esp+4: port (uint32_t)
		mov al,[esp+8]
		mov dx,[esp+4]
		out dx,al
		ret

; read byte from port
global _inb
_inb:
		; esp+4: port (uint32_t)
		; returns: uint8_t
		mov dx,[esp+4]
		in al,dx
		and eax,0x000000FF			; just in case
		ret

; breakpoint for bochs
global _breakpoint
_breakpoint:
		breakpoint
		ret

; Attempt to delay using some tricks
global _delay
_delay:
		mov ecx, 0x1FFFFF
	.loop:
		mov word [esp+4], 0x3D5
		call _inb
		dec ecx
		cmp ecx,0
		jne .loop
		ret

; 
; Load IDT
;
global _idt_load
_idt_load_00:
		push ebp
		mov ebp, esp
		push ebx
		
		mov ebx, [ebp+4]
		
		mov ax, [ebx]
		mov [.idtr_limit], ax
		
		mov eax, [ebx+2]
		mov [.idtr_offset], eax
		
		lidt [.idtr]

		pop ebx
		pop ebp
		ret
	
	.idtr:
	.idtr_limit: dw 0
	.idtr_offset: dd 0

;
; Load IDT
; Debug version
;
_idt_load:
		pusha

		; create entry for INT80
		;mov ebx, .idt+(0x0*8)				; ebx: base pointer to IDT entry (64 bits is 8 bytes)
		mov ebx, .idt
		
		%assign isrc 0
		%rep 32
			mov eax, __isr_%[isrc]
			mov word [ebx+0], ax				; offset_lo
			mov word [ebx+2], KERN_CODE_SEL		; kernel code selector
			mov byte [ebx+4], 0					; reserved
			mov byte [ebx+5], 0x8E				; present|priv_0|seg_0|gate_int32
			and eax, 0xFFFF0000
			shr eax, 16
			mov word [ebx+6], ax				; offset_hi
			add ebx, 8

			%assign isrc isrc+1
		%endrep
		
		;mov eax, __isr_0
		;mov word [ebx+0], ax				; offset_lo
		;mov word [ebx+2], KERN_CODE_SEL		; kernel code selector
		;mov byte [ebx+4], 0					; reserved
		;mov byte [ebx+5], 0x8E				; present|priv_0|seg_0|gate_int32
		;and eax, 0xFFFF0000
		;shr eax, 16
		;mov word [ebx+6], ax				; offset_hi

		; fill idtr
		mov eax, ebx
		sub eax, .idt						; eax=ebx-.idt
		dec eax								; eax--
		
		mov [.idtr_limit],  eax ;word (32*8)-1 ; word (32*8)		; They say we need at least 32 ints, and that we should not substract 1
		mov [.idtr_offset], dword .idt

		; load idtr
		lidt [.idtr]
		
		; enable interrupts
		breakpoint
		sti
		
		; try a divide by zero to test
		;mov cx, 0
		;div cx

		popa
		ret				; GPF here, motherfucker!

	.idtr:
	.idtr_limit: dw 0
	.idtr_offset: dd 0

	.idt: times 256 dq 0
	.idt_end:

;
; isr for INT80
;
global _isr80
_isr80:
		pusha
		push ds
		push es
		push fs
		push gs
		
		;mov ax, KERN_DATA_SEL
		;mov ds, ax
		;mov es, ax
		;mov fs, ax
		;mov gs, ax
		
		;mov [VGA_BASE], byte '*'
		mov ecx, 0
		mov dl, 0
		jmp _bsod
		
		pop gs
		pop fs
		pop es
		pop ds
		popa
		
		iret
		
		
; Macro for defining generic ISRs
; Params:
;  0: isr number
;  1: isr has error code?
%macro generic_isr 1
__isr_%1:
		mov dl, %1
		jmp _bsod
%endmacro

%assign isrc 0
%rep 32
	generic_isr isrc
	%assign isrc isrc+1
%endrep

;
; Blue screen of death
;
%define BSOD_COL (12|0x10)
_bsod:
		; Params
		;  ecx: error code
		;  dl: int number (byte)
		mov ebp, esp				; Stack of calling code

		mov ax, KERN_DATA_SEL
		mov ss, ax
		mov ds, ax
		mov ss, eax
		mov esp, .bsod_stack_end

	.write_error:
		mov ebx, VGA_BASE
		mov esi, .err_str1
		call .write_str
		
		mov eax, edx
		and eax, 0xF0
		shr eax, 4
		call .write_uint4
		
		mov eax, edx
		and eax, 0xF
		call .write_uint4
		
		mov esi, .err_str2
		call .write_str
		
	.halt:
		cli
		hlt
		jmp .halt
	.write_str:
		mov al, byte [esi]
		test al, al
		je .write_str_end
		mov byte [ebx], al
		mov byte [ebx+1], BSOD_COL 				; White text, blue background
		add ebx, 2
		inc esi
		jmp short .write_str
	.write_str_end:
		ret
	.write_uint4:
		mov al, [.hex_chars+eax]
		mov byte [ebx], al
		mov byte [ebx+1], BSOD_COL
		add ebx, 2
		ret

	.hex_chars: db '0123456789ABCDEF'
	.bsod_stack: times 256 db 0
	.bsod_stack_end:
	.err_str1: db 'Utter kernel failure: INT', 0
	.err_str2: db ' raised :(', 0

;
; Calls int 0x80
;
global _int80
_int80:
		int 0x80
		ret

;
; Enables ints
; 
global _sti
_sti:
		sti
		ret
		
;
; Disables ints
;
global _cli
_cli:
		cli
		ret




