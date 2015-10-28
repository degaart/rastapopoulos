;
;   0x7C00 - 0x7CFF: bootsect code
;   0x7B00 - 0x7BFF: bootsect stack
;
;   Call convention:
;       first param: ax
;       second param: cx
;       third param: dx
;       next params: stack, right-to-left
;       trashed regs: ax, cx, dx
;

    bits 16
    org 0x7C00
    %include 'bootsect.inc'

    ; ski BPB
    jmp short start

    ; BPB
    times 59 db 0

start:
    ; setup env
    cli
    mov ax,cs
    mov ds,ax
    mov ss,ax
    mov sp,0x7BFF

    ; dl=bios boot drive number
    mov [boot_device], dl

    xor dh, dh
    mov ax, dx
    call print_hex

    jmp halt


; SUBROUTINES
halt:
    hlt

; Output char at current cursor pos, and advance cursor
; Params: al: char to output
; bios int 0x10, ah=0xE, al=char_to_write,bh=0
print_char:
    push bx
    mov ah, 0xE
    xor bh, bh
    int 0x10
    pop bx
    ret

;
; write hex number into current cursor pos, with a leading 0x, and advance cursor
; Params: ax: number to write
; si: orig number
; ax: scratch
; bx: byte mask
; cl: shift
;
print_hex:
    push si
    push bx

    mov si, ax      ; save
    mov bx, 0xF000
    mov cl, 12

.loop:
    mov ax, si
    and ax, bx
    shr ax, cl
    cmp ax, 0xA
    jae .letter
    add ax, '0'
    jmp .emit
.letter:
    add ax, 'A'
    sub ax, 0xA
.emit:
    call print_char

    shr bx, 4
    sub cl, 4
    test bx, bx
    jz .return
    jmp .loop

.return:
    pop bx
    pop si
    ret

    ; padding for bios
    times 510-($-$$) db 0
    db 0x55,0xAA

