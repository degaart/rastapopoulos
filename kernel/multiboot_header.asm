; Constants
MBALIGN     equ 1 << 0                      ; page-align modules
MEMINFO     equ 1 << 1                      ; provide memmap
MBFLAGS     equ MBALIGN | MEMINFO
MAGIC       equ 0x1BADB002
CHECKSUM    equ -(MAGIC + MBFLAGS)

; Multiboot header
section .multiboot
align 4
    dd MAGIC
    dd MBFLAGS
    dd CHECKSUM

