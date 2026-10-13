#include "rasta.h"
#include "../kernel/syscall.h"

uint64_t sys_getticks(void)
{
    int64_t ticks = syscall64(SYSCALL_GETTICKS, 0, 0, 0, 0, 0);
    return ticks;
}

void sys_sleep(uint32_t ticks)
{
    syscall(SYSCALL_SLEEP, ticks, 0, 0, 0, 0);
}

int sys_key_down(unsigned scancode)
{
    return syscall(SYSCALL_KEYDOWN, scancode, 0, 0, 0, 0);
}

void sys_exit(void)
{
    syscall(SYSCALL_EXIT, 0, 0, 0, 0, 0);
    while (1)
        ;
}
