#include <rasta.h>
#include <syscall.h>

void rs_mmap(const void* addr, uint32_t physical, uint32_t flags) {
    rs_syscall(SYSCALL_MMAP, (uint32_t)addr, physical, flags);
}

