BITS 16
ORG 0x7C00

%define VGAOFFSET 0xA000

start:
    ; setup environment
    mov     ax, cs
    mov     ds, ax
    mov     es, ax
    mov     ss, ax
    mov     sp, 0x7BFF

    ; Set video mode
    mov     ah, 0x00            ; set video mode
    mov     al, 0x13            ; 320x200x8
    int     0x10

    ; Plot a pixel to the screen
    ; mov     ah, 0x0C
    ; mov     al, 256/2           ; color
    ; mov     cx, 320/2           ; x
    ; mov     dx, 200/2           ; y
    ; int     0x10
    mov     ax, 0xC
    mov     cx, 320/2
    mov     dx, 200/2
    call    putpixel
   
    cli
    hlt

; Write pixel into screen
; ax: color
; cx: x
; dx: y
putpixel:
    ; offset = 320*y + x;
    push    di
    push    ax
    push    dx

    mov     ax, 320
    mul     cx

    pop     dx
    add     ax, dx

    mov     di, VGAOFFSET
    add     di, ax
    
    pop     ax
    mov     [di], ax

    pop     di
    ret



; 0x55AA terminator
padding:
    times 510 - ($ - $$) db 0
    db 0x55, 0xAA






