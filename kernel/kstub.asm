;
; Rastapopoulos
; asm entry-point for kernel
;

section .text

kernel_base equ 0xC0000000


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

extern kmain
; void __log(const char* func, const char* file, int line, const char* fmt, ...);
extern __log

global _kernel_entry
_kernel_entry:
    ; Map first 4MB into kernel_base
    mov     ecx, initial_pagedir - kernel_base
    mov     cr3, ecx                            ; PDBR

    mov     ecx, cr4
    or      ecx, 0x10                           ; PSE
    mov     cr4, ecx

    mov     ecx, cr0
    or      ecx, 0x80000000                     ; PG
    mov     cr0, ecx

    lea     ecx, [.unmap_low]
    jmp     ecx

.unmap_low:
    ; Unmap low 4MB
    mov     DWORD [initial_pagedir], 0
    mov     ecx, cr3
    mov     cr3, ecx

.setup_stack:
    ; Setup kernel stack
    ; NOTE: The bootloader has already disabled interrupts
    mov     esp, initial_kernel_stack + 4096

    ; Check multiboot bootloader
    cmp     eax, 0x2BADB002
    jne     .not_multiboot

.start_kernel:
    ; Machine state
    ;‘EAX’
    ;     Must contain the magic value ‘0x2BADB002’; the presence of this value indicates to the operating system that it was loaded by a Multiboot-compliant boot loader (e.g. as opposed to another type of boot loader that the operating system can also be loaded from). 
    ; ‘EBX’
    ;     Must contain the 32-bit physical address of the Multiboot information structure provided by the boot loader (see Boot information format).
    ; ‘CS’
    ;     Must be a 32-bit read/execute code segment with an offset of ‘0’ and a limit of ‘0xFFFFFFFF’. The exact value is undefined.
    ; ‘DS’
    ; ‘ES’
    ; ‘FS’
    ; ‘GS’
    ; ‘SS’
    ;     Must be a 32-bit read/write data segment with an offset of ‘0’ and a limit of ‘0xFFFFFFFF’. The exact values are all undefined.
    ; ‘A20 gate’
    ;     Must be enabled.
    ; ‘CR0’
    ;     Bit 31 (PG) must be cleared. Bit 0 (PE) must be set. Other bits are all undefined.
    ; ‘EFLAGS’
    ;     Bit 17 (VM) must be cleared. Bit 9 (IF) must be cleared. Other bits are all undefined. 
    ; 
    ; All other processor registers and flag bits are undefined. This includes, in particular:
    ; 
    ; ‘ESP’
    ;     The OS image must create its own stack as soon as it needs one.
    ; ‘GDTR’
    ;     Even though the segment registers are set up as described above, the ‘GDTR’ may be invalid, so the OS image must not load any segment registers (even just reloading the same values!) until it sets up its own ‘GDT’.
    ; ‘IDTR’
    ;     The OS image must leave interrupts disabled until it sets up its own IDT. 

    push    ebx
    call    kmain
    jmp     .halt

.not_multiboot:
    mov     esi, str.not_multiboot
    mov     dx, 0xE9
.print_loop:
    mov     al, [esi]
    test    al, al
    jz      .halt
    out     dx, al
    inc     esi
    jmp     .print_loop

.halt:
    cli
    hlt


; void hlt()
global hlt
hlt:
    hlt
    ret

section .bss
align 4096
global initial_kernel_stack
initial_kernel_stack:
    resb    4096

section .rodata
str:
    .not_multiboot: db `Bootloader not multiboot-compliant\r\n\0`
    .hello: db `Hello, world!\r\n\0`

section .data
align 4096
initial_pagedir:
    dd 0x83                                     ; PS|RW|P for first 4MB
    times ((kernel_base >> 22) - 1) dd 0        ; Unmapped pages
    dd 0x83                                     ; 3GB
    times (1024 - (kernel_base >> 22)) - 1 dd 0 ; rest of address space



