;
; vim: syntax=asm:ft=asm:
;

%define KERNEL_BASE_ADDR    0xC0000000
%define INT10_ORG           0x7C00
%define INT10_SCRATCH       (INT10_ORG + 512)
%define INT10_STACK         (INT10_SCRATCH + 512)
%define INT10_SENTINEL      0xDEADBEEF

%define P_EAX               0x00
%define P_EBX               (P_EAX + 4)
%define P_ECX               (P_EBX + 4)
%define P_EDX               (P_ECX + 4)
%define P_EBP               (P_EDX + 4)
%define P_ESI               (P_EBP + 4)
%define P_EDI               (P_ESI + 4)
%define P_ES                (P_EDI + 4)
%define P_FS                (P_ES + 4)
%define P_GS                (P_FS + 4)
%define P_EFLAGS            (P_GS + 4)
%define P_ESP               (P_EFLAGS + 4)
%define P_RET               (P_ESP + 4)
%define P_CS                (P_RET + 4)
%define P_DS                (P_CS + 4)
%define P_IDTR              (P_DS + 4)
%define P_GDTR              (P_IDTR + 8)
%define P_SENTINEL          (P_GDTR + 8)

%define break               xchg bx, bx
