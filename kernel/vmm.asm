section .text

global invlpg
invlpg:
    mov eax, [esp + 4]
    invlpg [eax]
    ret


