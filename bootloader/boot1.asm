; vim: set ft=nasm:
bits 16
section .text

%define breakpoint xchg bx, bx
VGA_BASE equ 0xB8000
VGA_WIDTH equ 80
VGA_HEIGHT equ 25
SEG_CODE32 equ 0x08
SEG_DATA32 equ 0x10
SEG_CODE16 equ 0x18
SEG_DATA16 equ 0x20
STACKTOP equ 0x7C00
HEAP_START equ 0x500

struc rmode_regs
    .ax:        resw 1
    .bx:        resw 1
    .cx:        resw 1
    .dx:        resw 1
    .si:        resw 1
    .di:        resw 1
    .bp:        resw 1
    .es:        resw 1
    .flags:     resw 1
endstruc

struc memmap
    .base:      resq 1
    .length:    resq 1
    .type:      resd 1
    .attr:      resd 1
endstruc

entry:
    cli
    cld
    xor  ax, ax
    mov  ds, ax
    mov  es, ax
    mov  ss, ax
    mov  esp, STACKTOP
    jmp  0x0:entry2

entry2:
    mov  ax, VGA_BASE / 16
    mov  fs, ax

%macro DEBUG 1
    mov  [fs:0], byte %1
    mov  [fs:1], byte 0x1F
%endmacro
    ; detect memory using int 0x15, eax=0xE820
    ; this call returns invalid values for Packard Bell PB520R (Pentium)
    xor  ebx, ebx
get_memmap_E820:
    mov  eax, 0xE820
    mov  edi, [heap_start]
    mov  [edi+memmap.attr], dword 1
    mov  ecx, memmap_size
    add  [heap_start], dword memmap_size
    mov  edx, 'PAMS'
    clc
    int  0x15
    cli                                 ; this is needed or it hangs
    jc   .done
    DEBUG '0'

    cmp  eax, 'PAMS'
    jne  get_memmap_E801
    cmp  ebx, 0
    je   .done
    jmp  get_memmap_E820

.done:
    DEBUG '1'
    cmp  edi, HEAP_START            ; did we really get memmap?
    je   get_memmap_E801

    mov  [edi+memmap.base], dword 0
    mov  [edi+memmap.base+4], dword 0
    mov  [edi+memmap.length], dword 0
    mov  [edi+memmap.length+4], dword 0
    jmp  enable_a20

; get memmap using int 0X15, eax = 0xE801
; works on pentiums, but they all support get_memmap_E801 so this is unused
get_memmap_E801:
    DEBUG '2'
    
    mov  [heap_start], dword HEAP_START       ; reset heap
    mov  eax, 0xE801
    clc
    int  0x15
    cli
    jc   get_memmap_88
    DEBUG '3'

    ; first region: 0 - 0x9FC00 (640kb)
    mov  edi, [heap_start]
    mov  [edi+memmap.base], dword 0
    mov  [edi+memmap.base+4], dword 0
    mov  [edi+memmap.length], dword 0x9FC00
    mov  [edi+memmap.length+4], dword 0
    mov  [edi+memmap.type], dword 1
    mov  [edi+memmap.attr], dword 1
    add  edi, memmap_size

    ; second region: 1MB - AX * 1KB
    mov  [edi+memmap.base], dword 0x100000
    mov  [edi+memmap.base+4], dword 0
    and  eax, 0xFFFF
    shl  eax, 10                        ; eax *= 1024
    mov  [edi+memmap.length], eax
    mov  [edi+memmap.length+4], dword 0
    mov  [edi+memmap.type], dword 1
    mov  [edi+memmap.attr], dword 1
    add  edi, memmap_size

    ; third region: 16MB - BX * 64KB
    mov  [edi+memmap.base], dword 0x1000000
    mov  [edi+memmap.base+4], dword 0
    and  ebx, 0xFFFF
    shl  ebx, 16                        ; ebx *= 65536
    mov  [edi+memmap.length], ebx
    mov  [edi+memmap.length+4], dword 0
    mov  [edi+memmap.type], dword 1
    mov  [edi+memmap.attr], dword 1
    add  edi, memmap_size

    ; terminating block
    mov  [edi+memmap.base], dword 0
    mov  [edi+memmap.base], dword 0
    mov  [edi+memmap.length], dword 0
    mov  [edi+memmap.length+4], dword 0
    mov  [edi+memmap.type], dword 0
    mov  [edi+memmap.attr], dword 0
    add  edi, memmap_size
    jmp  enable_a20

; get memmap using int 0x15, ah=0x88
; this does not work after calling the other options :(
get_memmap_88:
    DEBUG '4'
    mov  [heap_start], dword HEAP_START
    mov  ah, 0x88
    clc                     ; some BIOS are really stupid and don't update
    int  0x15               ; carry flag... However, this call should never
    cli                     ; fail on 386+ machines
    jc   .error
    DEBUG '5'

    ; first region: 0 - 0x9FC00 (640kb)
    mov  edi, [heap_start]
    mov  [edi+memmap.base], dword 0
    mov  [edi+memmap.base+4], dword 0
    mov  [edi+memmap.length], dword 0x9FC00
    mov  [edi+memmap.length+4], dword 0
    mov  [edi+memmap.type], dword 1
    mov  [edi+memmap.attr], dword 1
    add  edi, memmap_size

    ; second region: 1MB - AX * 1KB
    mov  [edi+memmap.base], dword 0x100000
    mov  [edi+memmap.base+4], dword 0
    and  eax, 0xFFFF
    shl  eax, 10                        ; eax *= 1024
    mov  [edi+memmap.length], eax
    mov  [edi+memmap.length+4], dword 0
    mov  [edi+memmap.type], dword 1
    mov  [edi+memmap.attr], dword 1
    add  edi, memmap_size

    ; terminating block
    mov  [edi+memmap.base], dword 0
    mov  [edi+memmap.base], dword 0
    mov  [edi+memmap.length], dword 0
    mov  [edi+memmap.length+4], dword 0
    mov  [edi+memmap.type], dword 0
    mov  [edi+memmap.attr], dword 0
    add  edi, memmap_size
    jmp  enable_a20

.error:
    DEBUG '6'
    jmp  halt

enable_a20:
    ; Check A20 enabled
    call a20_enabled
    test ax, ax
    jnz  setup_gdt

    call kbd_wait
    push 0xAD               ; disable
    call kbd_send_command

    call kbd_wait
    push 0xD0               ; read
    call kbd_send_command

    call kbd_wait
    call kbd_get_data
    push ax

    call kbd_wait
    push 0xD1               ; write
    call kbd_send_command

    call kbd_wait
    pop  ax
    or   ax, 0x02
    push ax
    call kbd_send_data

    call kbd_wait
    push 0xAE               ; enable
    call kbd_send_command

    call a20_enabled
    test ax, ax
    jnz  setup_gdt

    push a20_error_message
    call print

    jmp  halt

setup_gdt:
    cli             ; some bios calls we did re-enabled interrupts
    push start_message
    call print
    lgdt [gdt]
    mov  eax, cr0
    or   eax, 1
    mov  cr0, eax
    jmp  SEG_CODE32:entry32

halt:
    cli
    hlt
    jmp  halt

; void print(const char* message)
print:
    push bp
    mov  bp, sp
    push bx
    push si

    mov  ah, 0x0E
    xor  bh, bh
    mov  bl, 0x07
    mov  si, [bp + 4]

.loop:
    mov  al, [si]
    test al, al
    je   .break
    int  0x10
    inc  si
    jmp  .loop

.break:
    pop  si
    pop  bx
    mov  sp, bp
    pop  bp
    ret  2

; unsigned a20_enabled()
;   compare byte at 0x0:0x500 (phys 0x500) and 0xffff:0x510 (phys 0x100500)
a20_enabled:
    push ds
    push es
    push si
    push di

    ; ds:si 0x0:0x500
    xor  ax, ax
    mov  ds, ax
    mov  si, 0x500

    ; es:di 0xFFFF:0x510
    mov  ax, 0xFFFF
    mov  es, ax
    mov  di, 0x510

    ; save old values
    xor  ax, ax
    mov  al, [ds:si]
    push ax

    mov  al, [es:di]
    push ax

    ; overwrite them with different values
    mov  byte [ds:si], 0x0F
    mov  byte [es:di], 0xF0

    ; then overwrite ds:si
    mov  byte [ds:si], 0x07

    ; and check es:di
    mov  cl, [es:di]

    ; restore them values
    pop  ax
    mov  [es:di], al
    pop  ax
    mov  [ds:si], al

    ; if cl != 0x07, then A20 enabled
    cmp  cl, 0x07
    jne  .enabled

    xor  ax, ax
    jmp  .return

.enabled:
    mov  ax, 1

.return:
    pop  di
    pop  si
    pop  es
    pop  ds
    ret

; void kbd_wait()
kbd_wait:
    in   al, 0x64
    test al, 0x02
    jnz  kbd_wait
    ret

; void kbd_send_command(word command)
kbd_send_command:
    push bp
    mov  bp, sp
    mov  al, [bp + 4] 
    out  0x64, al
    pop  bp
    ret  2

; word kbd_get_data()
kbd_get_data:
    in   al, 0x64
    test al, 0x01
    jz   kbd_get_data
    ret  2

; void kbd_send_data(word data)
kbd_send_data:
    push bp
    mov  bp, sp
    mov  al, [bp + 4]
    out  0x60, al
    pop  bp
    ret  2

real_mode_thunk:
    mov  ax, SEG_DATA16
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax

    mov  eax, cr0
    and  eax, ~1
    mov  cr0, eax

    mov  ax, 0
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax
    jmp  0x00:.real_mode_entry

.real_mode_entry:
    lidt [rm_idt]

    push ebp
    mov  ebp, esp
    push ebx
    push esi
    push edi

    ; ; debuggging
    ; mov  ax, VGA_BASE / 16
    ; mov  es, ax
    ; mov  bx, (80*4)
    ; mov  [es:bx], byte 'X'
    ; mov  [es:bx+1], byte 0x1E

    mov  bp, [bp + 8]
    mov  ax, [bp + rmode_regs.ax]
    mov  bx, [bp + rmode_regs.bx]
    mov  cx, [bp + rmode_regs.cx]
    mov  dx, [bp + rmode_regs.dx]
    mov  si, [bp + rmode_regs.si]
    mov  di, [bp + rmode_regs.di]
    mov  es, [bp + rmode_regs.es]

    push ebp
    mov  bp, [bp + rmode_regs.bp]
    int  0x13
    cli                 ; pcem/86box reenable interrupts after int 0x13
    cld                 ; just to be sure
    pop  ebp

    mov  [bp + rmode_regs.ax], ax
    mov  [bp + rmode_regs.bx], bx
    mov  [bp + rmode_regs.cx], cx
    mov  [bp + rmode_regs.dx], dx
    mov  [bp + rmode_regs.si], si
    mov  [bp + rmode_regs.di], di
    pushf
    pop  word [bp + rmode_regs.flags]

    pop  edi
    pop  esi
    pop  ebx
    pop  ebp

    lgdt [gdt]
    mov  eax, cr0
    or   eax, 1
    mov  cr0, eax
    jmp  SEG_CODE32:int13.return

;=========================================================================
; 32-bit code
;=========================================================================
section .text
bits 32
entry32:
    ; setup segment registers and stack
    mov  ax, SEG_DATA32     ; data segment
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax
    mov  esp, STACKTOP

    ; call C entry point
    push dword HEAP_START
    extern start
    call start
    add  esp, 4

halt32:
    cli
    hlt
    jmp halt32

; void int13(struct regs* regs)
;   Deactivates protected mode then calls int13 with the specified parameters
global int13
int13:
    jmp  SEG_CODE16:real_mode_thunk
.return:
    mov  ax, SEG_DATA32
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax
    ret  4

; Interrupt handlers
%assign isr_index 0
%rep 33
    isr_stub_ %+ isr_index:
        mov  ebx, isr_index
        jmp  isr_common_stub
    %assign isr_index isr_index+1
%endrep

isr_common_stub:
    mov  edi, VGA_BASE
    mov  esi, isr_stub_message
.loop:
    mov  al, [esi]
    mov  [edi], al
    mov  [edi+1], byte 0x1F
    add  edi, 2
    inc  esi
    test al, al
    jnz  .loop

    cmp  ebx, 10
    jb   .onedigit

    xor  edx, edx
    mov  eax, ebx
    mov  ecx, 10
    div  ecx                ; eax: first digit, edx: second digit

    add  al, '0'
    mov  [edi], al
    mov  [edi+1], byte 0x1F
    add  dl, '0'
    mov  [edi+2], dl
    mov  [edi+3], byte 0x1F
    jmp  halt32

.onedigit:
    add  bl, '0'
    mov  [edi], bl
    mov  [edi+1], byte 0x1F
    jmp  halt32

section .rodata
start_message: db "RastapopoulOS 2nd stage bootloader", 13, 10, 0
a20_error_message: db "Failed to enable A20 line", 13, 10
isr_stub_message: db "UNHANDLED INTERRUPT ", 0
rm_idt:
    dw 0x03FF
    dd 0
memmap_head: dd 0
memmap_err_message1: db "Memory detection failed (CF set)", 13, 10, 0
memmap_err_message2: db "Memory detection failed (EAX != 'SMAP')", 13, 10, 0
memmap_err_message3: db "Memory detection 0xE801 failed", 13, 10, 0
memmap_err_message4: db "Memory detection 0x88 failed", 13, 10, 0
memmap_ok_message1: db "Memory detection OK", 13, 10, 0
heap_start: dd HEAP_START

global isr_stub_table
isr_stub_table:
    %assign isr_index 0
    %rep 33
        dd isr_stub_ %+ isr_index
        %assign isr_index isr_index+1
    %endrep

gdt:
    dw gdt_entries.end - gdt_entries -1
    dd gdt_entries

    GDT_ACCESS_PRESENT      equ (1 << 7)
    GDT_ACCESS_DPL0         equ 0
    GDT_ACCESS_TYPE_NORMAL  equ (1 << 4)
    GDT_ACCESS_EXEC         equ (1 << 3)
    GDT_ACCESS_CONFORMING   equ (1 << 2)
    GDT_ACCESS_DIRECTION    equ (1 << 2)
    GDT_ACCESS_READABLE     equ (1 << 1)
    GDT_ACCESS_WRITABLE     equ (1 << 1)
    GDT_ACCESS_ACCESSED     equ (1)

    GDT_FLAG_GRAN4K         equ (1 << 3)
    GDT_FLAG_SIZE16         equ 0
    GDT_FLAG_SIZE32         equ (1 << 2)

    ; base, limit, access, flags
    %macro gdt_entry 4
        dd (%2 & 0xFFFF) | ((%1 & 0xFFFF) << 16)
        dd \
            ((%1 >> 15) & 0xFF) | \
            ((%3 & 0xFF) << 8) | \
            (((%2 >> 16) & 0xF) << 16) | \
            ((%4 & 0xF) << 20) | \
            (((%1 >> 24) & 0xFF) << 24)
    %endmacro

gdt_entries:
    ; 0x00 null segment
    dd 0, 0

    ; 0x08 code segment
    gdt_entry 0, \
        0xFFFFF, \
        GDT_ACCESS_PRESENT|GDT_ACCESS_DPL0|GDT_ACCESS_TYPE_NORMAL| \
        GDT_ACCESS_EXEC|GDT_ACCESS_READABLE, \
        GDT_FLAG_GRAN4K|GDT_FLAG_SIZE32

    ; 0x10 data segment
    gdt_entry 0, \
        0xFFFFF, \
        GDT_ACCESS_PRESENT|GDT_ACCESS_DPL0|GDT_ACCESS_TYPE_NORMAL| \
        GDT_ACCESS_WRITABLE, \
        GDT_FLAG_GRAN4K|GDT_FLAG_SIZE32

    ; 0x18 16-bit code segment
    gdt_entry 0, \
        0xFFFFF, \
        GDT_ACCESS_PRESENT|GDT_ACCESS_DPL0|GDT_ACCESS_TYPE_NORMAL| \
        GDT_ACCESS_EXEC|GDT_ACCESS_READABLE, \
        GDT_FLAG_SIZE16

    ; 0x20 16-bit data segment
    gdt_entry 0, \
        0xFFFFF, \
        GDT_ACCESS_PRESENT|GDT_ACCESS_DPL0|GDT_ACCESS_TYPE_NORMAL| \
        GDT_ACCESS_WRITABLE, \
        GDT_FLAG_SIZE16

.end:


