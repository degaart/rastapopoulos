#include "syscall.h"
#include "util.h"
#include "io.h"
#include "debug.h"
#include "syscall_nums.h"
#include "process.h"

void Syscall::syscall_handler(const isr_regs_t* regs) {
    uint32_t func = regs->eax;
    uint32_t param0 = regs->ebx;
    uint32_t param1 = regs->ecx;
    uint32_t param2 = regs->edx;

    switch(func) {
        case SYSCALL_HALT:
            syscall_halt(param0, param1, param2);
            break;
        case SYSCALL_WRITE:
            syscall_write(param0, param1, param2);
            break;
        case SYSCALL_YIELD:
            syscall_yield(param0, param1, param2);
            break;
        case SYSCALL_EXIT:
            syscall_exit(param0, param1, param2);
            break;
        default:
            PANIC("Unhandled syscall 0x%X", func);
    }
}

void Syscall::init() {
    IDT::install_handler(0x80, syscall_handler, true);
}

uint32_t Syscall::syscall_halt(uint32_t param0, uint32_t param1, uint32_t param2) {
    halt();
    return 0;
}

uint32_t Syscall::syscall_yield(uint32_t param0, uint32_t param1, uint32_t param2) {
    sti();
    yield();
    return 0;
}

uint32_t Syscall::syscall_write(uint32_t param0, uint32_t param1, uint32_t param2) {
    TRACE("<ring3>: %s", (const char*)param0);
    return 0;
}

uint32_t Syscall::syscall_exit(uint32_t, uint32_t, uint32_t) {
    Process::exit_current_process();
    return 0;
}
