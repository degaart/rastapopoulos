bits 16

section .text

; bool a20_enabled(void)
global a20_enabled
a20_enabled:
    push si
    push di
    push ds
    push es
    cli

    mov ax, 0x0000                  ;   0x0000:0x0500(0x00000500) -> ds:si
    mov ds, ax
    mov si, 0x0500

    not ax                          ;   0xffff:0x0510(0x00100500) -> es:di
    mov es, ax
    mov di, 0x0510

    mov al, [ds:si]                 ;   save old values
    mov byte [BufferBelowMB], al
    mov al, [es:di]
    mov byte [BufferOverMB], al

    mov ah, 1                       ;   check byte [0x00100500] == byte [0x0500]
    mov byte [ds:si], 0
    mov byte [es:di], 1
    mov al, [ds:si]
    cmp al, [es:di]
    jne .exit
    dec ah
.exit:
    mov al, [BufferBelowMB]
    mov [ds:si], al
    mov al, [BufferOverMB]
    mov [es:di], al
    shr ax, 8
    pop es
    pop ds
    pop di
    pop si
    ret

section .data
    BufferBelowMB: db 0
    BufferOverMB:   db 0

