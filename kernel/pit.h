#ifndef _PIT_H_
#define _PIT_H_

#include <stdint.h>
#include "idt.h"

class PIT {
private:
    static const int PORT_COMMAND = 0x43;
    static const int PORT_DATA = 0x40;

    static const int ICW = 0x36;

    static const int INTERNAL_FREQ = 1193180;
    static const int FREQ = 25; /* hz */

    static uint32_t _ticks;
    static void irq_handler(int irq, const isr_regs_t* regs);
public:
    static void init();
};

#endif

