bits 16
section .text

struc Regs
    .ax     resw 1
    .bx     resw 1
    .cx     resw 1
    .dx     resw 1
    .si     resw 1
    .di     resw 1
    .bp     resw 1
    .es     resw 1
    .flags  resw 1
endstruc

global bioscall
; void bioscall(uint16_t number, struct Regs* regs)
bioscall:
    push bp
    mov  bp, sp

    ; gcc-ia16 callee-save
    push bx
    push si
    push di
    push bp

    ; [bp+4] number
    ; [bp+6] regs
    mov  ax, [.intcall]              ; debug, remove when ready
    mov  al, [bp+4]
    mov  [.intcall+1], al

    mov  bx, [bp+6]
    push word [bx+Regs.es]
    push word [bx+Regs.bp]

    mov  ax, [bx+Regs.ax]
    mov  cx, [bx+Regs.cx]
    mov  dx, [bx+Regs.dx]
    mov  si, [bx+Regs.si]
    mov  di, [bx+Regs.di]
    mov  bx, [bx+Regs.bx]

    pop  bp
    pop  es

.intcall:
    int  0xcc           ; we will patch this :)

    pushf
    push es
    push bp
    push di
    push si
    push dx
    push cx
    push bx
    push ax

    ; Stack:
    ; sp+0  ax
    ; sp+2  bx
    ; sp+4  cx
    ; sp+6  dx
    ; sp+8  si
    ; sp+10 di
    ; sp+12 bp
    ; sp+14 es
    ; sp+16 flags
    ; sp+18 saved bp
    ; sp+20 saved di
    ; sp+22 saved si
    ; sp+24 saved bx
    ; sp+26 saved bp
    ; sp+28 return addr
    ; sp+30 number
    ; sp+32 regs
    mov  bp, sp
    mov  di, [bp+32]
    mov  ax, [bp+0]
    mov  [di+Regs.ax], ax
    mov  ax, [bp+2]
    mov  [di+Regs.bx], ax
    mov  ax, [bp+4]
    mov  [di+Regs.cx], ax
    mov  ax, [bp+6]
    mov  [di+Regs.dx], ax
    mov  ax, [bp+8]
    mov  [di+Regs.si], ax
    mov  ax, [bp+10]
    mov  [di+Regs.di], ax
    mov  ax, [bp+12]
    mov  [di+Regs.bp], ax
    mov  ax, [bp+14]
    mov  [di+Regs.es], ax
    pushf
    pop  ax
    mov  [di+Regs.flags], ax

    ; discard the 9 pushed words
    add  sp, 18

    pop  bp
    pop  di
    pop  si
    pop  bx
    pop  bp
    ret

