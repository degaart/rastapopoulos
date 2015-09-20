global gdt_flush
gdt_flush:
    mov eax, [esp+4]        ; should be pointer to gdt_ptr in gdt.cpp
    lgdt [eax]

    mov ax, 0x10            ; kernel data segment descriptor
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.return        ; 0x08: kernel code segment descriptor
.return:
    ret
