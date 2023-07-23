section .text

struc task
    .entry  resd 1
    .stack  resd 1
    .eflags resd 1
    .ebx    resd 1
    .esp    resd 1
    .ebp    resd 1
    .esi    resd 1
    .edi    resd 1
    .eip    resd 1
endstruc

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
    mov  [eax + task.esp], esp

    ; load esp of next task
    mov  eax, [esp + 20]
    mov  esp, [eax + task.esp]
    
    ; update current_task
    mov  [current_task], eax

    ; I guess we can just return now?
    ; assuming we already set new task's stack correctly
    pop  ebp
    pop  edi
    pop  esi
    pop  ebx
    ret

