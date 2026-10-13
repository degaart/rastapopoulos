section .text

%define number ebp+8
%define param1 ebp+12
%define param2 ebp+16
%define param3 ebp+20
%define param4 ebp+24
%define param5 ebp+28

; int32_t syscall(uint32_t number,
;                 uint32_t param1,
;                 uint32_t param2,
;                 uint32_t param3,
;                 uint32_t param4,
;                 uint32_t param5);
global syscall:function
syscall:
    push ebp
    mov  ebp, esp

    push ebx
    push esi
    push edi

    mov  eax, [number]
    mov  ebx, [param1]
    mov  ecx, [param2]
    mov  edx, [param3]
    mov  esi, [param4]
    mov  edi, [param5]
    int  0x80

    pop  edi
    pop  esi
    pop  ebx
    pop  ebp
    ret

