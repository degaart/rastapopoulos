section .text

extern current_task
global switch_task
switch_task:
    xchg bx, bx

    ; save state of current task
    push ebx
    push esi
    push edi
    push ebp

    ; save current esp into current task's data
    mov  eax, [current_task]
    mov  [eax], esp

    ; load esp of next task
    mov  eax, [esp + 20]
    mov  esp, [eax]
    
    ; update current_task
    mov  [current_task], eax

    ; I guess we can just return now?
    ; assuming we already set new task's stack correctly
    pop  ebp
    pop  edi
    pop  esi
    pop  ebx
    ret

