#ifndef _IDT_H_
#define _IDT_H_

#include <stdint.h>

struct isr_regs_t {
    uint32_t ds;                            // Data segment selector
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha.
    uint32_t int_no, err_code;              // Interrupt number and error code (if applicable)
    uint32_t eip, cs, eflags, useresp, ss;  // Pushed by the processor automatically.
} __attribute__((packed));

class IDT {
public:
    static void init();
    static void flush();
    static void dump();

    typedef void (*isr_handler_t)(const isr_regs_t* regs);
    static void install_handler(int num, isr_handler_t handler);
private:
    static void set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
};


#endif //_IDT_H_
