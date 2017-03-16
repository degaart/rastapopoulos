section .text

; uint32_t cmpxchg(volatile uint32_t* dest, uint32_t exchange, uint32_t compare)
global cmpxchg
cmpxchg:
    mov             edx, [esp + 4]
    mov             ecx, [esp + 8]
    mov             eax, [esp + 12]
    
    ; compare eax with [esp + 4]
    ; if equal, ecx is loaded into [esp + 4]
    ; else load [esp + 4] into eax
    lock cmpxchg    [edx], ecx
    ret

