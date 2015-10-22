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
    static void syscall_handler(const isr_regs_t* regs);
    
    static uint32_t syscall_halt(uint32_t, uint32_t, uint32_t);
    static uint32_t syscall_yield(uint32_t, uint32_t, uint32_t);
    static uint32_t syscall_write(uint32_t, uint32_t, uint32_t);
};

#endif //_SYSCAL_H_



