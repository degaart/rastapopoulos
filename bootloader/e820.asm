bits 16
section .text

; ascii for 'SMAP'
%define SMAP 0x534d4150

; bool e820(uint32_t* cookie, void* buf, uint16_t len);
global e820
e820:
    push bp
    mov  bp, sp

    push ebx
    push di

    ; [bp+4]   cookie
    ; [bp+6]   buf
    ; [bp+8]   len
    mov  eax, 0xe820
    mov  di, [bp+4]
    mov  ebx, [di]
    xor  ecx, ecx
    mov  cx, [bp+8]
    mov  edx, SMAP
    mov  di, [bp+6]
    int  0x15
    jc   .err

    ; Some BIOSes stop returning SMAP even on success
    ; we don't know if the returned map is valid, so just
    ; return error
    cmp  eax, SMAP
    jne  .err

    mov  di, [bp+4]
    mov  [di], ebx
    mov  ax, 1
    jmp  .return

.err:
    xor  ax, ax

.return:
    pop  di
    pop  ebx
    pop  bp
    ret

