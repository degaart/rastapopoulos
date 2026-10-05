bits 16
section .text

; void exec_kernel(uint32_t entry)
; [bp+4] = entry
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

    mov  esp, 0x80000           ; end of conventional memory

    and  ebp, 0xffff
    jmp  [ebp+4]

halt:
    jmp  halt

