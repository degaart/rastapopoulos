#include <stdint.h>

void syscall(uint32_t function, uint32_t param0, uint32_t param1, uint32_t param2);

/*
 * Fun starts here
 */
int main() {
    syscall(0x1, (uint32_t) "CAN HAZ CHEEZBURGER?", 0, 0);
    while(1);
    return 0;
}
