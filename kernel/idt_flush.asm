global idt_flush
idt_flush:
    mov eax, [esp+4]  ; Get the pointer to the idt_ptr, passed as a parameter.
    lidt [eax]        ; Load the IDT pointer.
    ret
