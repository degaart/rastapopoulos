#include "syscall.h"
#include "util.h"
#include "io.h"
#include "debug.h"
#include "syscall_nums.h"
#include "process.h"
#include "vmm.h"
#include "process.h"

#define SYSCALL(num, func) \
    case num:   \
        ret = func(param0, param1, param2); \
        break

void Syscall::syscall_handler(isr_regs_t* regs) {
    uint32_t func = regs->eax;
    uint32_t param0 = regs->ebx;
    uint32_t param1 = regs->ecx;
    uint32_t param2 = regs->edx;

    uint32_t ret;
    switch(func) {
        SYSCALL(SYSCALL_HALT, syscall_halt);
        SYSCALL(SYSCALL_TRACE, syscall_trace);
        SYSCALL(SYSCALL_YIELD, syscall_yield);
        SYSCALL(SYSCALL_EXIT, syscall_exit);
        SYSCALL(SYSCALL_MMAP, syscall_mmap);
        SYSCALL(SYSCALL_OUTB, syscall_outb);
        SYSCALL(SYSCALL_INB, syscall_inb);
        SYSCALL(SYSCALL_PORT_OPEN, syscall_port_open);
        SYSCALL(SYSCALL_PORT_CLOSE, syscall_port_close);
        SYSCALL(SYSCALL_PORT_SEND, syscall_port_send);
        SYSCALL(SYSCALL_PORT_READ, syscall_port_read);
        default:
            PANIC("Unhandled syscall 0x%X", func);
    }

    regs->eax = ret;
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

uint32_t Syscall::syscall_trace(uint32_t param0, uint32_t param1, uint32_t param2) {
    Process* proc = Process::current_process();
    assert(proc != nullptr);

    TRACE("<%s>: %s", proc->name(), (const char*)param0);
    return 0;
}

uint32_t Syscall::syscall_exit(uint32_t, uint32_t, uint32_t) {
    Process::exit_current_process();
    return 0;
}

uint32_t Syscall::syscall_mmap(uint32_t va, uint32_t pa, uint32_t flags) {
    if(va >= (uint32_t)_TEXT_START_ && va < 0x400000) {
        PANIC("Forbidden virtual address: 0x%X", va);
    }

    if(flags != VMM::PAGE_WRITABLE) {
        PANIC("Forbidden flags: 0x%X", flags);
    }

    flags |= VMM::PAGE_PRESENT | VMM::PAGE_USER;
    VMM::map(va, pa, flags);
    return 0;
}

uint32_t Syscall::syscall_outb(uint32_t port, uint32_t ch, uint32_t) {
    // TRACE("outb(0x%X, 0x%X)", port, ch);
    outb(port & 0xFFFF, ch & 0xFF);
    return 0;
}

uint32_t Syscall::syscall_inb(uint32_t port, uint32_t, uint32_t) {
    uint32_t ret = inb(port);
    // TRACE("inb(0x%X) -> 0x%X", port, ret);
    return ret;
}

uint32_t Syscall::syscall_port_send(uint32_t param0, uint32_t param1, uint32_t param2) {
    uint32_t port_number = param0;
    const Message_t* msg = (const Message_t*)param1;

    Process* proc = Process::process_for_port(port_number);
    if(proc == nullptr)
        return false;

    Port* port = proc->get_port(port_number);
    if(port == nullptr)
        return false;

    port->send(*msg);
    return true;
}

uint32_t Syscall::syscall_port_read(uint32_t param0, uint32_t param1, uint32_t param2) {
    uint32_t port_number = param0;
    Message_t* buffer = (Message_t*)param1;

    Process* proc = Process::current_process();
    Port* port = proc->get_port(port_number);
    if(!port) {
        return false;
    }

    while(port->empty()) {
        sti();
        yield();
        cli();
    }
    bool got_message = port->read(buffer);
    assert(got_message);
    return true;
}

uint32_t Syscall::syscall_port_open(uint32_t param0, uint32_t, uint32_t) {
    uint32_t port_number = param0;

    Process* proc = Process::current_process();
    assert(proc);

    return proc->open_port(port_number);
}

uint32_t Syscall::syscall_port_close(uint32_t param0, uint32_t, uint32_t) {
    uint32_t port_number = param0;

    Process* proc = Process::current_process();
    assert(proc);

    return proc->close_port(port_number);
}




