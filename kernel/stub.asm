;
; Rastapopoulos
; asm entry-point for kernel
;
; Memory layout:
;   0x500	    - 0x5FF     : kernel params, set by bootloader
;   0x6BFF	    - 0x7BFF    : initial kernel stack
;   0x100000	- ?         : kernel code
;

section .text
extern _BSS_START_
extern _BSS_END_
extern _DATA_START_
extern _DATA_END_

extern main

global _kernel_entry
_kernel_entry:
    ; Setup kernel stack
    ; NOTE: The bootloader has already disabled interrupts
    ; Caveat: do not use the stack before BSS zeroed (as the stack is stored in the bss)
    xchg bx, bx
    mov esp, _initial_kernel_stack + 4096

    ; Check multiboot bootloader
    cmp eax, 0x2BADB002
    jne .not_multiboot

    ; 
    ; zero kernel BSS
    mov     eax, _BSS_START_
.loop:
    cmp     eax, _BSS_END_
    jae     .start_kernel
    mov     [eax], DWORD 0x00000000
    add     eax, 0x4
    jmp    .loop

.start_kernel:
    ; Call C entry point, with pushed ebx as entry-point
    push ebx
    call main

.not_multiboot:
    mov esi, str.not_multiboot
    mov dx, 0xE9
.print_loop:
    mov al, [esi]
    test al, al
    jz .halt
    out dx, al
    inc esi
    jmp .print_loop

.halt:
    cli
    hlt

; multiboot header
align 4
multiboot_header:
    ; 0   u32     magic   required
    ; 4   u32     flags   required
    ; 8   u32     checksum    required
    ; 12  u32     header_addr     if flags[16] is set
    ; 16  u32     load_addr   if flags[16] is set
    ; 20  u32     load_end_addr   if flags[16] is set
    ; 24  u32     bss_end_addr    if flags[16] is set
    ; 28  u32     entry_addr  if flags[16] is set
    ; 32  u32     mode_type   if flags[2] is set
    ; 36  u32     width   if flags[2] is set
    ; 40  u32     height  if flags[2] is set
    ; 44  u32     depth   if flags[2] is set 

    MB_MAGIC                equ 0x1BADB002
    MB_ALIGN_MODULES        equ (1)
    MB_MEMMAP               equ (1 << 1)
    MB_VIDEOMODES           equ (1 << 2)
    MB_ADDRFIELDS           equ (1 << 16)

    FLAGS                   equ (MB_ALIGN_MODULES|MB_MEMMAP)

    dd MB_MAGIC
    dd FLAGS
    dd -(MB_MAGIC + FLAGS)

section .bss
_initial_kernel_stack:
    resb 4096

section .rodata
str:
    .not_multiboot: db `Bootloader not multiboot-compliant\r\n\0`

