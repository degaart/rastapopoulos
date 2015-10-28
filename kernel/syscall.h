#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include <stdint.h>
#include "idt.h"

class Syscall {
public:
    static void init();
    
    typedef uint32_t (*handler_t)(uint32_t param0, uint32_t param1, uint32_t param2);
    static void install_handler(uint32_t function, handler_t handler);
private:
    static void syscall_handler(isr_regs_t* regs);
    
    static uint32_t syscall_halt(uint32_t, uint32_t, uint32_t);
    static uint32_t syscall_yield(uint32_t, uint32_t, uint32_t);
    static uint32_t syscall_trace(uint32_t, uint32_t, uint32_t);
    static uint32_t syscall_exit(uint32_t, uint32_t, uint32_t);
    static uint32_t syscall_mmap(uint32_t, uint32_t, uint32_t);
    static uint32_t syscall_outb(uint32_t, uint32_t, uint32_t);
    static uint32_t syscall_inb(uint32_t, uint32_t, uint32_t);
    static uint32_t syscall_outw(uint32_t, uint32_t, uint32_t);
    static uint32_t syscall_inw(uint32_t, uint32_t, uint32_t);
    static uint32_t syscall_port_send(uint32_t, uint32_t, uint32_t);
    static uint32_t syscall_port_read(uint32_t, uint32_t, uint32_t);
    static uint32_t syscall_port_open(uint32_t, uint32_t, uint32_t);
    static uint32_t syscall_port_close(uint32_t, uint32_t, uint32_t);
};

#endif //_SYSCAL_H_



