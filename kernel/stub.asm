;
; Rastapopoulos
; asm entry-point for kernel
;
; Memory layout:
;   0x500	    - 0x5FF     : kernel params, set by bootloader
;   0x6BFF	    - 0x7BFF    : initial kernel stack
;   0x100000	- ?         : kernel code
;
extern _BSS_START_
extern _BSS_END_

extern main

global _kernel_entry
_kernel_entry:
    ; setup kernel stack
    ; Note: we assume the bootloader has correctly set up
    ; data and stack segments here
    cli
    mov     esp, 0x7BFF

    ; zero kernel BSS
    mov     eax, _BSS_START_
.loop:
    cmp     eax, _BSS_END_
    jae     .start_kernel
    mov     [eax], DWORD 0x00000000
    add     eax, 0x4
    jmp    .loop

.start_kernel:
    ; Jump to C entry point
    jmp main
    
