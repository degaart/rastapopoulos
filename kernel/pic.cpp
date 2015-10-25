#include "pic.h"
#include "io.h"
#include "debug.h"
#include "util.h"
#include "gdt.h"
#include "string.h"

PIC::irq_handler PIC::_irq_handlers[16];

void PIC::init() {
    assert(sizeof(_irq_handlers) > sizeof(irq_handler));
    bzero(_irq_handlers, sizeof(_irq_handlers));

    /* init PIC0 & PIC1 (ICW1) */
    outb(PIC0_COMMAND, COMMAND_ICW1_IC4|COMMAND_ICW1_INIT);
    outb(PIC1_COMMAND, COMMAND_ICW1_IC4|COMMAND_ICW1_INIT);

    /* map interrupt vectors (ICW2) */
    outb(PIC0_DATA, 0x20);              /* IRQ0-IRQ7 => int 0x20 - 0x27 */
    outb(PIC1_DATA, 0x28);              /* IRQ8-IRQ16 => int 0x38-0x30 */

    /* ICW3 (cascading configuration) */
    outb(PIC0_DATA, 1 << 2);            /* slave pic connected to irq2 */
    outb(PIC1_DATA, 0x2);               /* yep, this slave pic is indeed connected to irq 0x2 */

    /* ICW4 */
    outb(PIC0_DATA, DATA_ICW4_8086);
    outb(PIC1_DATA, DATA_ICW4_8086);

    /* Clear data regs */
    outb(PIC0_DATA, 0);
    outb(PIC1_DATA, 0);

    /* Install interrupt handler */
    IDT::install_handler(0x20, irq_stub);
}

void PIC::irq_stub(isr_regs_t* regs) {
    /*
        TODO: Check that this function is really reentrant, 
        as the IRQ is acknowledged before calling the handler function
        (so the handler function need not to return)
    */
    int irq = regs->int_no - 0x20;
    eoi(irq);

    if(_irq_handlers[irq]) {
        _irq_handlers[irq](irq, regs);
    } else {
        TRACE("WARNING: Unhandled IRQ %d", irq);
    }

    // TRACE("IRQ%d triggerred", regs->int_no - 0x20);
}

void PIC::eoi(unsigned irq) {
    assert(irq < 16);
    if(irq > 8)
        IO::outb(PIC1_COMMAND, COMMAND_EOI);
    IO::outb(PIC0_COMMAND, COMMAND_EOI);
}

void PIC::install_irq_handler(int irq, irq_handler handler) {
    _irq_handlers[irq] = handler;
}

void PIC::remove_irq_handler(int irq, irq_handler handler) {
    _irq_handlers[irq] = nullptr;
}

