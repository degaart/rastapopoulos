    ; debugging macros
%macro print_char 1
    mov al, %1
    mov ah, 0x0e
    mov bx, 0x0007
    int 0x10
%endmacro

%macro isabuggerR 1
    mov al, 0x02
    out 0x7a, al
    mov al, %1
    out 0x7b, al
%endmacro

%macro isabuggerL 1
    mov al, 0x04
    out 0x7a, al
    mov al, %1
    out 0x7b, al
%endmacro

%macro debugcon 0-1
    %if %0 = 1
        mov al, %1
    %endif
    out 0xe9, al
%endmacro

