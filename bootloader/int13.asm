bits 16

section .text

global int13

; struct Regs offsets
%define REGS_AX     0
%define REGS_BX     2
%define REGS_CX     4
%define REGS_DX     6
%define REGS_SI     8
%define REGS_DI    10
%define REGS_BP    12
%define REGS_ES    14
%define REGS_CF    16

; void int13(struct Regs *regs)
; stack:
;   [SP + 0] = return address
;   [SP + 2] = regs
int13:
    push bp
    mov  bp, sp

    ; [bp + 0] = saved BP
    ; [bp + 2] = return address
    ; [bp + 4] = regs
    mov  bx, [bp + 4]

    push word [bx + REGS_ES]
    push word [bx + REGS_BP]

    mov  ax, [bx + REGS_AX]
    mov  cx, [bx + REGS_CX]
    mov  dx, [bx + REGS_DX]
    mov  si, [bx + REGS_SI]
    mov  di, [bx + REGS_DI]
    mov  bx, [bx + REGS_BX]

    pop  bp
    pop  es

    int  0x13

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
    ;
    ; SP+0   AX
    ; SP+2   BX
    ; SP+4   CX
    ; SP+6   DX
    ; SP+8   SI
    ; SP+10  DI
    ; SP+12  BP
    ; SP+14  ES
    ; SP+16  FLAGS
    ; SP+18  saved BP
    ; SP+20  return address
    ; SP+22  regs

    mov  bp, sp
    mov  di, [bp + 22]
    mov  ax, [bp + 0]
    mov  [di + REGS_AX], ax
    mov  ax, [bp + 2]
    mov  [di + REGS_BX], ax
    mov  ax, [bp + 4]
    mov  [di + REGS_CX], ax
    mov  ax, [bp + 6]
    mov  [di + REGS_DX], ax
    mov  ax, [bp + 8]
    mov  [di + REGS_SI], ax
    mov  ax, [bp + 10]
    mov  [di + REGS_DI], ax
    mov  ax, [bp + 12]
    mov  [di + REGS_BP], ax
    mov  ax, [bp + 14]
    mov  [di + REGS_ES], ax

    ; CF -> FLAGS bit 0
    mov  ax, [bp + 16]
    and  ax, 1
    mov  [di + REGS_CF], ax

    ; discard result from stack (9 words)
    add  sp, 18

    pop  bp
    ret

