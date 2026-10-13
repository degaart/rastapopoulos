#include "rasta.h"
#include "../kernel/syscall.h"

uint64_t sys_getticks(void)
{
    int64_t ticks = syscall64(0, 0, 0, 0, 0, 0);
    return ticks;
}

void sys_sleep(uint32_t ticks)
{
    syscall(1, ticks, 0, 0, 0, 0);
}

int sys_key_down(unsigned scancode)
{
    return syscall(2, scancode, 0, 0, 0, 0);
}

