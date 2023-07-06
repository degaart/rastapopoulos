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

entry:
    cli
    cld
    xor  ax, ax
    mov  ds, ax
    mov  es, ax
    mov  ss, ax
    mov  sp, 0x7C00
    jmp  0x0:entry2

entry2:
    ; Check A20 enabled
    call a20_enabled
    test ax, ax
    jnz  setup_gdt

    ; query bios A20 support
    mov  ax, 0x2403
    int  0x15
    jb   enable_a20_kbd
    test ah, ah
    jnz  enable_a20_kbd

    ; query A20 status
    mov  ax, 0x2402
    int  0x15
    jb   enable_a20_kbd
    test ah, ah
    jnz  enable_a20_kbd

    ; try enabling it using bios function
    mov  ax, 0x2401
    int  0x15

    call a20_enabled
    test ax, ax
    jnz  setup_gdt

enable_a20_kbd:
    push a20_kbd_message
    call print

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

    push a20_fast_message
    call print

    ; enable via fast method
    in   al, 0x92
    or   al, 2
    out  0x92, al
    call a20_enabled
    test ax, ax
    jnz  setup_gdt
    
    jmp  halt

setup_gdt:
    push gdt_message
    call print
    lgdt [gdt]
    mov  eax, cr0
    or   al, 1
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
    push ebp
    mov  ebp, esp
    push ebx
    push esi
    push edi

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

    ; restore protected mode
    mov  eax, cr0
    or   al, 1
    mov  cr0, eax
    jmp  SEG_CODE32:int13.return

;=========================================================================
; 32-bit code
;=========================================================================
section .text
bits 32
entry32:
    ; setup segment registers and stack
    mov  ax, SEG_DATA32   ; data segment
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax
    mov  esp, 0x7C00

    ; init text-mode cursor
    call cursor_pos32
    mov  [cursor_x], edx
    mov  [cursor_y], eax

    ; print a message
    push pmode_message
    call print32

    ; ; get back to real mode
    ; sub  esp, rmode_regs_size
    ; mov  ebx, esp
    ; mov  [ebx + rmode_regs.ax], word 0x1301
    ; mov  [ebx + rmode_regs.bx], word 0x0007
    ; mov  [ebx + rmode_regs.cx], word 25
    ; mov  [ebx + rmode_regs.dx], word 0x0102
    ; mov  [ebx + rmode_regs.si], word 0
    ; mov  [ebx + rmode_regs.di], word 0
    ; mov  [ebx + rmode_regs.bp], word rmode_message
    ; mov  [ebx + rmode_regs.es], word 0
    ; push ebx
    ; mov  esi, 0xDEADBEEF
    ; mov  edi, 0xBADB00B5
    ; call int13
    ; add  esp, rmode_regs_size

    ; call C entry point
    extern start
    call start

halt32:
    cli
    hlt
    jmp halt32

; long long cursor_pos32()
;   eax -> y
;   edx -> x
cursor_pos32:
    mov  dx, 0x03D4
    mov  al, 0x0F
    out  dx, al
    mov  dx, 0x03D5
    in   al, dx
    mov  cl, al                 ; cl -> x

    mov  dx, 0x03D4
    mov  al, 0x0E
    out  dx, al
    mov  dx, 0x03D5
    in   al, dx
    mov  ch, al                 ; ch -> y

    xor  dx, dx
    mov  ax, cx
    mov  cx, VGA_WIDTH
    div  cx                     ; ax: y, dx: x
    and  eax, 0xFFFF
    and  edx, 0xFFFF
    ret

; void set_cursor_pos32(unsigned x, unsigned y)
set_cursor_pos32:
    push ebp
    mov  ebp, esp

    ; uint16_t pos = y * VGA_WIDTH + x;
    xor  dx, dx
    mov  ax, [ebp + 12]
    mov  cx, VGA_WIDTH
    mul  cx
    add  ax, [ebp + 8]
    mov  cx, ax

    ; outb(0x3D4, 0x0F);
    mov  dx, 0x3D4
    mov  al, 0x0F
    out  dx, al

    ; outb(0x3D5, (uint8_t) (pos & 0xFF));
    mov  dx, 0x3D5
    mov  ax, cx
    and  ax, 0xFF
    out  dx, al

    ; outb(0x3D4, 0x0E);
    mov  dx, 0x3D4
    mov  al, 0x0E
    out  dx, ax

    ; outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
    mov  dx, 0x3D5
    mov  ax, cx
    shr  ax, 8
    out  dx, al

    pop  ebp
    ret  8

; void putchar32(dword x, dword y, dword char, dword attr)
;   write char at particular position, without updating cursor position
putchar32:
    push ebp
    mov  ebp, esp

    mov  edx, [ebp + 12]                ; y
    shl  edx, 6
    mov  eax, [ebp + 12]                ; y
    shl  eax, 4
    add  eax, edx
    add  eax, [ebp + 8]                 ; x
    shl  eax, 1
    add  eax, VGA_BASE

    mov  dl, [ebp + 16]                 ; char
    mov  dh, [ebp + 20]                 ; attr

    mov  [eax], dl
    mov  [eax+1], dh

    mov  esp, ebp
    pop  ebp
    ret 16

; void writechar32(dword char, dword attr)
;   write char at current cursor position, and advance cursor by 1 char
;       ebp + 12    attr
;       ebp + 8     char
;       esp + 4     cursor_x
;       esp + 8     cursor_y
global writechar32
writechar32:
    push ebp
    mov  ebp, esp

    ; handle CR/LF
    mov  al, [ebp + 8]
    cmp  al, 13
    je   .cr
    cmp  al, 10
    je   .lf

    ; write char at that position
    push dword [ebp + 12]               ; attr
    push dword [ebp + 8]                ; char
    push dword [cursor_y]               ; y
    push dword [cursor_x]               ; x
    call putchar32

    ; advance cursor right
    inc  dword [cursor_x]
    cmp  dword [cursor_x], VGA_WIDTH
    jb   .setpos

    ; advance cursor next line
    ; if(cursor_y < VGA_HEIGHT - 1)
    ;   cursor_y++
    ; else
    ;   scroll32
    mov  dword [cursor_x], 0
    cmp  dword [cursor_y], VGA_HEIGHT - 1
    jae  .scroll1

    inc  dword [cursor_y]
    jmp  .setpos

    ; scroll
.scroll1:
    call scroll32
    jmp  .setpos

.cr:
    ; move cursor to start of line
    mov  dword [cursor_x], 0
    jmp  .setpos

.lf:
    ; move cursor to next line
    ; if(cursor_y < VGA_HEIGHT - 1)
    ;   cursor_y++
    ; else
    ;   scroll32
    cmp  dword [cursor_y], VGA_HEIGHT - 1
    jae  .scroll2

    inc  dword [cursor_y]
    jmp  .setpos

    ; scroll
.scroll2:
    call scroll32

.setpos:
    push dword [cursor_y]
    push dword [cursor_x]
    call set_cursor_pos32

.return:
    mov  esp, ebp
    pop  ebp
    ret 8

; void print32(void* s)
global print32
print32:
    push ebp
    mov  ebp, esp
    push esi

    mov  esi, [ebp + 8]

.next_char:
    xor  eax, eax
    mov  al, [esi]
    test al, al
    jz   .return

    push dword 0x07
    push eax
    call writechar32

    inc  esi
    jmp  .next_char

.return:
    pop  esi
    pop  ebp
    ret 4

; void memcpy32(dword dst, dword src, dword len)
memcpy32:
    push esi
    push edi
    mov  ecx, [esp + 20]
    mov  esi, [esp + 16]
    mov  edi, [esp + 12]
    rep  movsb
    pop  edi
    pop  esi
    ret 12

; void delay32(dword amount)
delay32:
    mov  ecx, [esp + 4]
    xor  al, al
    mov  dx, 0x80
.repeat:
    out  dx, al
    loop .repeat
    ret  4

; void scroll32()
;   memcpy(VGA_BASE, VGA_BASE + (VGA_WIDTH * 2), VGA_WIDTH * VGA_HEIGHT * 2)
scroll32:
    mov  eax, VGA_WIDTH
    mov  ecx, VGA_HEIGHT
    mul  ecx
    shl  eax, 1
    push eax                    ; len
    
    mov  eax, VGA_WIDTH
    shl  eax, 1
    add  eax, VGA_BASE
    push eax                    ; src

    push dword VGA_BASE         ; dst
    call memcpy32
    ret

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

section .rodata
cursor_x: dd 0
cursor_y: dd 0
enabled_message: db "A20 gate is enabled", 13, 10, 0
disabled_message: db "A20 gate is disabled", 13, 10, 0
gdt_message: db "Creating initial GDT", 13, 10, 0
a20_kbd_message: db "Enabling A20 (kbd method)", 13, 10, 0
a20_fast_message: db "Enabling A20 (fast method)", 13, 10, 0
pmode_message: db "Entered 32-bit protected mode", 13, 10, 0
rmode_message: db "Back to real-mode again", 13, 10, 0

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


