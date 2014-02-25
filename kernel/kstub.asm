; RastaPopoul 0S 0.09
;
; Assembly stub for kernel
;
bits 32

extern kmain
extern write_debug

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

		; zero bss section of kernel memory
		extern _bss_start
		extern _kernel_end
		mov esi, _bss_start
	.init_bss:
		mov dword [esi], 0
		add esi, 4
		cmp esi, _kernel_end
		jb .init_bss
		
		; call kernel entry point
		call kmain
		jmp _halt

; halt processor
global _halt
_halt:
		nop
		cli
		hlt
		jmp _halt

;
; Halt processor (does not disable interrupts)
; may return to calling code
;
global _ihalt
_ihalt:
		hlt
		ret
	
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

;
; Somehow call user-mode
; Params:
;	DWORD ss	ebp+8
; 	DWORD esp	ebp+12
;	DWORD cs	ebp+16
;	DWORD eip	ebp+20
global _call_usermode
_call_usermode:
		breakpoint
		push ebp
		mov ebp, esp

		cli
		mov ax, [ebp+8]
		mov ds, ax
		mov es, ax
		mov fs, ax
		mov gs, ax

		; This is silly! cs and ds are pushed as motherfucking DWORDs!!!
		;pushf
		;pop eax
		;or eax, 0x200				; set if flags in usermode

		pushf
		pop eax
		push dword [ebp+8]			; ss, as DWORD
		push dword [ebp+12]			; esp
		push eax					; flags
		push dword [ebp+16]			; cs
		push dword [ebp+20]			; da proc
		iret
		jmp $						; normally, we should't get here

global _usermode_entry:
_usermode_entry:
		breakpoint

		; we can call kernel with this
		int 0x80
		jmp _usermode_entry

;
; Load new GDT
; Params:	ebp+8	physical address of new GDT
;			ebp+12	size of new GDT
global _gdt_load
_gdt_load:
		push ebp
		mov ebp, esp
		
		mov eax, [ebp+8]
		mov [.gdt_offset], eax
		mov eax, [ebp+12]
		dec eax
		mov [.gdt_limit], ax
		
		lgdt [.gdtr]
		
		pop ebp
		ret

	.gdtr:
	.gdt_limit: dw 0
	.gdt_offset: dd 0

global _tss_load
_tss_load:
		mov ax, [esp+4]
		ltr ax
		ret

;
; Load IDT
; Debug version
;
global _idt_load
_idt_load:
		push ebp
		mov ebp, esp
		pusha
		
		; clear isr table (cleaner this way)
		mov edi , __isr_table
	.clear_isr_table:
		mov dword [edi], 0
		add edi, 4
		cmp edi, __isr_table+(256*4)
		jb .clear_isr_table
		
		; clear idt
		mov ebx, .idt
	.clear_idt:
		mov dword [ebx], 0
		add ebx, 4
		cmp ebx, .idt_end
		jb .clear_idt

		; move each entry into __isr_table
		mov edi, __isr_table
		mov esi, [ebp+8]					; struct IDT_ENTRY*
		mov ecx, [ebp+12]					; count
	.move_entries:
		cmp ecx, 0
		jz .load_idt
		
		mov eax, [esi]						; handler address
		mov [edi], eax
		
		add esi, 8
		add edi, 4
		dec ecx
		jmp .move_entries
	.load_idt:
		mov ebx, .idt						; IDT entry base
		mov esi, [ebp+8]					; entries
		mov edx, [ebp+12]					; count
		mov ecx, 0							; current entry
		
		; We can lookup the thunk in the
		; __isr_thunk_table
	.load_idt_loop:
		cmp ecx, edx
		je  .load_idtr

		; if entry in __isr_thunk is NULL, we don't bother
		; adding it to idt
		cmp dword [esi], 0
		jz .next_idt_entry

		mov eax, ecx						; eax=index
		shl eax, 2							; eax=index*4
		add eax, __isr_thunk_table			; eax=__isr_thunk_table+(index*4)
		
		mov eax, [eax]
		mov word [ebx+0], ax				; offset_lo
		mov word [ebx+2], KERN_CODE_SEL		; kernel code selector
		mov byte [ebx+4], 0					; reserved
		and eax, 0xFFFF0000
		shr eax, 16
		mov word [ebx+6], ax				; offset_hi
		mov ax, [esi+4]						; attributes
		mov byte [ebx+5], al				; 

	.next_idt_entry:
		inc ecx
		add esi, 8
		add ebx, 8
		jmp .load_idt_loop

	.load_idtr:
		; fill idtr
		mov eax, ebx
		sub eax, .idt						; eax=ebx-.idt
		dec eax								; eax--
		
		mov [.idtr_limit],  ax				; idt size
		mov [.idtr_offset], dword .idt		; idt address

		; load idtr
		lidt [.idtr]
		
		popa
		pop ebp
		ret

	.idtr:
	.idtr_limit: dw 0
	.idtr_offset: dd 0

	align 8
	.idt: times 256 dq 0
	.idt_end:

align 4
__isr_table: times 256 dd 0
%include 'isr_stub.inc'

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

;
; get flags register
;
global _getflags
_getflags:
		pushf
		pop eax
		ret


;
; Control registers accessors
;
%macro CR_ACCESSOR 1
global _read_%1
_read_%1:
		mov eax, %1
		ret

global _write_%1
_write_%1:
		mov eax, [esp+4]
		mov %1, eax
		ret
%endmacro

CR_ACCESSOR cr0
CR_ACCESSOR cr2
CR_ACCESSOR cr3
CR_ACCESSOR cr4
