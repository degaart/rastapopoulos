;
; Rastapopoulos
; asm entry-point for kernel
;

extern main

_kernel_entry:
	cli
	hlt

	jmp main


