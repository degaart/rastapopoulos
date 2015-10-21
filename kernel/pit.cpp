#include "pit.h"
#include "pic.h"
#include "idt.h"
#include "io.h"
#include "debug.h"
#include "util.h"
#include "timer.h"

void PIT::init() {
    uint32_t divisor = INTERNAL_FREQ / FREQ;

    outb(PORT_COMMAND, ICW);
    outb(PORT_DATA, LOBYTE(divisor));
    outb(PORT_DATA, HIBYTE(divisor));

    PIC::install_irq_handler(PIC::IRQ_TIMER, irq_handler);
}

void PIT::irq_handler(int irq, const isr_regs_t* regs) {
    Timer::on_tick(regs);
}
