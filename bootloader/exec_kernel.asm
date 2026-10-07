bits 16
section .text

; void exec_kernel(uint32_t entry, const void* multiboot_info)
; [bp+4] = entry
; [bp+8] = multiboot_info
;
; Assumes A20 is alreay enabled and GDT is already
; set up correctly with 0x08 pointing to the code segment
; and 0x10 pointing to the data segment
global exec_kernel
exec_kernel:
    push bp,
    mov  bp, sp

    cli

    ; enable CR0.PM
    mov  eax, cr0
    or   eax, 1
    mov  cr0, eax

    jmp  0x08:pm_entry

bits 32
pm_entry:
    ; setup segment registers
    mov  ax, 0x10
    mov  ds, ax
    mov  es, ax,
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax

    and  esp, 0xffff
    and  ebp, 0xffff
    mov  eax, 0x2BADB002            ; multiboot signature
    movzx ebx, WORD [ebp+8]
    jmp  [ebp+4]

halt:
    jmp  halt

