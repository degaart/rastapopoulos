;
; Rastapopoulos
; asm entry-point for kernel
;
; Memory layout:
;   0x500	    - 0x5FF     : kernel params, set by bootloader
;   0x6BFF	    - 0x7BFF    : initial kernel stack
;   0x100000	- ?         : kernel code
;
extern main
_kernel_entry:
    ; setup kernel stack
    ; Note: we assume the bootloader has correctly set up
    ; data and stack segments here
    mov     esp, 0x7BFF

    ; Jump to C entry point
    jmp main
    