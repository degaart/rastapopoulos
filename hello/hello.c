#include <stdint.h>

/*
 * Fun starts here
 */
void syscall(uint32_t function, uint32_t param0, uint32_t param1, uint32_t param2) {
    asm(
        ".intel_syntax noprefix\n"
        "mov eax, [ebp+8]\n"
        "mov ebx, [ebp+12]\n"
        "mov ecx, [ebp+16]\n"
        "mov edx, [ebp+20]\n"
        "int 0x80\n"
        ::: "eax", "ebx", "ecx", "edx", "esi", "edi", "memory"
    );
}

int main() {
    syscall(0x1, (uint32_t) "CAN HAZ CHEEZBURGER?", 0, 0);
    while(1);
    return 0;
}

