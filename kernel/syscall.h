#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include <stdint.h>
#include "idt.h"
#include "dynamic_array.h"
#include "syscall_nums.h"

class Syscall {
public:
    static void init();
    
    typedef uint32_t (*handler_t)(uint32_t param0, uint32_t param1, uint32_t param2);
    static void install_handler(uint32_t function, handler_t handler);
private:
    static bool _initialized;
    static DynamicArray<handler_t> _handlers;

    static void syscall_handler(isr_regs_t* regs);
};

#endif //_SYSCAL_H_



