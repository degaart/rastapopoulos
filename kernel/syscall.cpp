#include "syscall.h"
#include "util.h"
#include "io.h"
#include "debug.h"
#include "syscall_nums.h"
#include "process.h"
#include "vmm.h"
#include "process.h"

bool Syscall::_initialized = false;
DynamicArray<Syscall::handler_t> Syscall::_handlers;

void Syscall::int_syscall(isr_regs_t* regs) {
    uint32_t func = regs->eax;
    uint32_t param0 = regs->ebx;
    uint32_t param1 = regs->ecx;
    uint32_t param2 = regs->edx;

    if(func >= _handlers.size()) {
        PANIC("Unhandled syscall 0x%X", func);
    }
    uint32_t ret = _handlers[func](param0, param1, param2, regs);
    regs->eax = ret;
}

void Syscall::init() {
    assert(!_initialized);

    IDT::install_handler(0x80, int_syscall, true);
    _initialized = true;
}

void Syscall::install_handler(uint32_t function, Syscall::handler_t handler) {
    if(_handlers.size() < function + 1) 
        _handlers.resize(function + 1);

    _handlers[function] = handler;
}

uint32_t Syscall::syscall(uint32_t func, uint32_t param0, uint32_t param1, uint32_t param2) {
    int ret;
    asm volatile(
        "int $0x80\n"
        "mov %0, %%eax\n"
        : "=r"(ret)
        : "a"(func), "b"(param0), "c"(param1), "d"(param2)
    );
    return ret;
}
