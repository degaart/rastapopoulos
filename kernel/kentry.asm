;
; asm entry-point for kernel
; https://wiki.osdev.org/Bare_Bones_with_NASM
;

MBALIGN  equ  1 << 0            ; align loaded modules on page boundaries
MEMINFO  equ  1 << 1            ; provide memory map
FLAGS    equ  MBALIGN | MEMINFO ; this is the Multiboot 'flag' field
MAGIC    equ  0x1BADB002        ; 'magic number' lets bootloader find the header
CHECKSUM equ -(MAGIC + FLAGS)   ; checksum of above, to prove we are multiboot

KERNEL_BASE equ 0xC0000000

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .bss

; stack
; marked as nobits in linker file so is not stored in kernel image
; must be 16-bytes aligned due to sysv abi
align 16
stack_bottom: resb 16384                  ; 16kb
stack_top:

; initial pagedir and pagetable
; kernel should reside at 0xC0100000
align 4096
initial_pagedir: resd 1024
initial_pagetable: resd 1024

; store multiboot_info pointer here
multiboot_info: resd 1

global stack_bottom:data
global stack_top:data
global initial_pagetable:data
global initial_pagedir:data

section .text

; entry point called by bootloader
; marked as a function
global _start:function (_start.end - _start)
_start:
    ; environment set up by bootloader:
    ;   - 32-bit protected mode
    ;   - interrupts disabled
    ;   - paging disabled
    ;   - undefined stack
    ;   - undefined gdt

    ; save multiboot info
    mov [multiboot_info], ebx
    
    ; setup initial pagedir
    ; Identity map 0x00000000 - 0x003FFFFF
    ; Then map     0xC0000000 - 0xC03FFFFF to 0x00000000 - 0x003FFFFF
    ; Finally, put recursive directory entry

    ; initial_pagedir[0] = (initial_pagetable & 0xFFFFF000)|(PDE_PRESENT|PDE_WRITABLE)
    mov eax, initial_pagetable
    and eax, 0xFFFFF000
    or  eax, 1 | (1 << 1)
    mov DWORD [initial_pagedir], eax    ; initial_pagedir[0] = eax

    ; initial_pagedir[0xC0000000>>22] = eax
    mov ecx, 0xC0000000
    shr ecx, 22
    shl ecx, 2                      ; ecx *= 2
    add ecx, initial_pagedir
    mov DWORD [ecx], eax

    ; initial_pagedir[1023] = initial_pagedir|(1|(1<<1))
    mov eax, initial_pagedir
    and eax, 0xFFFFF000
    or  eax, 1 | (1 << 1)
    mov ecx, 1023
    shl ecx, 2
    add ecx, initial_pagedir
    mov DWORD [ecx], eax

    ; pagetables
    mov esi, initial_pagetable
    mov ebx, 0                          ; current_page

.loop:
    mov eax, ebx
    and eax, 0xFFFFF000
    or  eax, 1 | (1 << 1)
    mov DWORD [esi], eax

    add ebx, 0x1000
    add esi, 4

    cmp ebx, 0x400000
    jb  .loop

    ; enable paging
    mov eax, initial_pagedir
    mov cr3, eax
    mov eax, cr0
    or  eax, (1 << 31)
    mov cr0, eax

    ; setup stack
    mov esp, stack_top
    sub esp, 16

    ; call kernel C entry point
    mov eax, [multiboot_info]
    mov [esp], eax
    extern kmain
    call kmain

    ; if main returned, halt
    cli
.hang:
    hlt
    jmp .hang

.end:

