		use16
		org 0x7C00
		
		%define breakpoint xchg bx, bx
		
		jmp 0:start

start:
		; cs=ds=ss=0
		; sp=0xFFD6

		; enable A20
		mov ax, 0x2401
		int 0x15
		
		; Enter protected mode with flat address space
		cli          		; disable interrupts
		lgdt [gdt_desc]  	; load GDT register with start address of Global Descriptor Table
		mov eax, cr0
		or al, 1     		; set PE (Protection Enable) bit in CR0 (Control Register 0)
		mov cr0, eax
 
		; Perform far jump to selector 08h (offset into GDT, pointing at a 32bit PM code segment descriptor) 
		; to load CS with proper PM32 descriptor)
 		jmp dword 0x08:start32
 		jmp halt

halt:
		cli
		hlt
		jmp halt
		
start32:
		; Now we are in 32-bit protected mode with a flat
		; address space
		bits 32
		breakpoint
		
		; cs=0x8
		; ds=ss: 0x0
		; esp: FFD6
		
		; Setup registers
		mov ax,0x10
		mov ds,ax
		mov ss,ax
		mov esp,0x9FBFF
		jmp halt32
halt32:
		cli
		hlt
		jmp halt32
		
gdt_desc:
		.size: dw (3*8)-1		; gdt size!!! do not fucking substract 1
		;.size: dw 2*8			; gdt size!!! do not fucking substract 1
		.offset: dd gdt

gdt:
		.gdt_null: db 0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00
		.gdt_kcode: db 0xFF,0xFF,0x00,0x00,0x00,0x9A,0xCF,0x00
		.gdt_kdata: db 0xFF,0xFF,0x00,0x00,0x00,0x92,0xCF,0x00
		
		; it triple-faults if we omit gdt_tss
		;.gdt_tss: db 0xFF,0xFF,0x00,0x00,0x00,0x89,0xCF,0x00
gdt_end:

padding:
		times 510-($-$$) db 90
		db 0x55, 0xAA
