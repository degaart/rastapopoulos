section .text

%define ring3_eip ebp+8
%define ring3_esp ebp+12
%define ring3_cs  ebp+16
%define ring3_ds  ebp+20

; void ring3_jump(void* ring3_eip,
;                 void* ring3_esp,
;                 uint32_t ring3_cs,
;                 uint32_t ring3_ds)
; __attribute__((noreturn))
global ring3_jump:function
ring3_jump:
    push ebp
    mov  ebp, esp

    cli
    mov  eax, [ring3_ds]
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    push eax                        ; ss
    mov  eax, [ring3_esp]
    push eax                        ; esp
    pushf
    pop  eax
    or   eax, 0x200                 ; IF
    push eax                        ; eflags
    mov  eax, [ring3_cs]
    push eax                        ; cs
    mov  eax, [ring3_eip]
    push eax                        ; eip

    iret
    db 0xcc

