section .text

; uint32_t read_cr3()
global read_cr3
read_cr3:
    mov     eax, cr3
    ret

; uint32_t read_cr2()
global read_cr2
read_cr2:
    mov     eax, cr2
    ret

; uint32_t read_cr1()
global read_cr1
read_cr1:
    mov     eax, cr1
    ret

; uint32_t read_cr0()
global read_cr0
read_cr0:
    mov     eax, cr0
    ret

; uint32_t read_eflags()
global read_eflags
read_eflags:
    pushf
    pop     eax
    ret

; uint32_t read_ebp()
global read_ebp
read_ebp:
    mov     eax, ebp
    ret

; uint32_t read_esp()
global read_esp
read_esp:
    mov     eax, esp
    sub     eax, 4          ; 4 bytes for return address
    ret

; uint32_t read_esi()
global read_esi
read_esi:
    mov     eax, esi
    ret

; void write_cr3(uint32_t val)
global write_cr3
write_cr3:
    push    ebp
    mov     ebp, esp
    mov     eax, [ebp + 8]
    mov     cr3, eax
    pop     ebp
    ret

; void write_cr2(uint32_t val)
global write_cr2
write_cr2:
    push    ebp
    mov     ebp, esp
    mov     eax, [ebp + 8]
    mov     cr2, eax
    pop     ebp
    ret

; void write_cr1(uint32_t val)
global write_cr1
write_cr1:
    push    ebp
    mov     ebp, esp
    mov     eax, [ebp + 8]
    mov     cr1, eax
    pop     ebp
    ret

; void write_cr0(uint32_t val)
global write_cr0
write_cr0:
    push    ebp
    mov     ebp, esp
    mov     eax, [ebp + 8]
    mov     cr0, eax
    pop     ebp
    ret

; void write_eflags(uint32_t val)
global write_eflags
write_eflags:
    push    ebp
    mov     ebp, esp
    push    DWORD [ebp + 8]
    popf
    pop     ebp
    ret

