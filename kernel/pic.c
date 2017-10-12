#include "pic.h"
#include "io.h"
#include "debug.h"
#include "string.h"
#include "locks.h"
#include "registers.h"

#define PIC0_COMMAND            0x20
#define PIC0_DATA               0x21
#define PIC1_COMMAND            0xA0
#define PIC1_DATA               0xA1

#define COMMAND_EOI             0x20

#define COMMAND_ICW1_IC4        0x01           /* If set(1), the PIC expects to recieve IC4 during initialization. */
#define COMMAND_ICW1_SNGL       0x02           /* If set(1), only one PIC in system. If cleared, PIC is cascaded with slave PICs, and ICW3 must be sent to controller. */
#define COMMAND_ICW1_ADI        0x04           /*  If set (1), CALL address interval is 4, else 8 */
#define COMMAND_ICW1_LTIM       0x08           /* If set (1), Operate in Level Triggered Mode. If Not set (0), Operate in Edge Triggered Mode */
#define COMMAND_ICW1_INIT       0x10           /* Initialization */

#define COMMAND_OCW3_ISR        0x0b            /* Read in-service register */

#define DATA_ICW4_8086          0x01           /* 8086/88 (MCS-80/85) mode */
#define DATA_ICW4_AUTO          0x02           /* Auto (normal) EOI */
#define DATA_CW4_BUF_SLAVE      0x08           /* Buffered mode/slave */
#define DATA_ICW4_BUF_MASTER    0x0C           /* Buffered mode/master */
#define DATA_ICW4_SFNM          0x10           /* Special fully nested (not) */

#define IRQ_PIT         0
#define IRQ_KBD         1
#define IRQ_CASCADE     2
#define IRQ_COM2        3
#define IRQ_COM1        4
#define IRQ_LPT2        5
#define IRQ_FDD         6
#define IRQ_LPT1        7
#define IRQ_RTC         8
#define IRQ_FREE1       9
#define IRQ_FREE2       10
#define IRQ_FREE3       11
#define IRQ_PSMOUSE     12
#define IRQ_FPU         13
#define IRQ_ATA1        14
#define IRQ_ATA2        15

static void irq_stub(struct isr_regs* regs);

static irq_handler_t irq_handlers[16];

void pic_init()
{
    bzero(irq_handlers, sizeof(irq_handlers));

    /* init PIC0 & PIC1 (ICW1) */
    outb(PIC0_COMMAND, COMMAND_ICW1_IC4|COMMAND_ICW1_INIT);
    outb(PIC1_COMMAND, COMMAND_ICW1_IC4|COMMAND_ICW1_INIT);

    /* map interrupt vectors (ICW2) */
    outb(PIC0_DATA, 0x20);              /* IRQ0-IRQ7 => int 0x20 - 0x27 */
    outb(PIC1_DATA, 0x28);              /* IRQ8-IRQ16 => int 0x28 - 0x30 */

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
    for(unsigned irq = 0x20; irq < 0x31; irq++)
        idt_install(irq, irq_stub, false);
}

void pic_install(int irq, irq_handler_t handler)
{
    enter_critical_section();

    irq_handlers[irq] = handler;

    leave_critical_section();
}

void pic_remove(int irq)
{
    enter_critical_section();
    irq_handlers[irq] = NULL;
    leave_critical_section();
}

static unsigned pic_read_reg(int ocw3)
{
    outb(PIC0_COMMAND, ocw3);
    outb(PIC1_COMMAND, ocw3);

    unsigned pic0_val = inb(PIC0_COMMAND);
    unsigned pic1_val = inb(PIC1_COMMAND);
    unsigned result = pic0_val | (pic1_val << 8);
    return result;
}

static unsigned pic_get_isr()
{
    unsigned isr = pic_read_reg(COMMAND_OCW3_ISR);
    return isr;
}

static void eoi(unsigned irq)
{
    assert(irq < 16);
    if(irq > 8)
        outb(PIC1_COMMAND, COMMAND_EOI);
    outb(PIC0_COMMAND, COMMAND_EOI);
}

/*
 * IRQ handling
 * On receiving an IRQ, mask the IRQ and then ack it
 * After the handler acknowledges the IRQ, unmask it
 */
static void irq_stub(struct isr_regs* regs)
{
    assert(!interrupts_enabled());

    static unsigned warn_unhandled = 0xFFFFFFFF;

    /*
     * Acknowledge the IRQ after calling the handler, except the timer,
     * as the timer may not return
     * TODO: Check and handle spurious IRQ7
     */
    int irq = regs->int_no - 0x20;

    /* Handle spurious irqs */
    if(irq == 7) {
        if(!(pic_get_isr() & (1 << 7))) {
            trace("WARNING: Spurious IRQ7");
            return;
        }
    } else if(irq == 15) {
        unsigned isr = pic_get_isr();
        if(!(isr & (1 << 15))) {
            trace("WARNING: Spurious IRQ15");
            eoi(IRQ_CASCADE);
            return;
        }
    }

    if(irq == 0)
        eoi(irq);

    if(irq_handlers[irq]) {
        irq_handlers[irq](irq, regs);
        warn_unhandled |= (1 << irq);
    } else {
#define X(T) case T: trace("WARNING: Unhandled IRQ%d (%s)", T, #T); break
        // Only warn once about unhandled interrupts
        if(warn_unhandled & (1 << irq)) {
            switch(irq) {
                X(IRQ_PIT);
                X(IRQ_KBD);
                X(IRQ_CASCADE);
                X(IRQ_COM2);
                X(IRQ_COM1);
                X(IRQ_LPT2);
                X(IRQ_FDD);
                X(IRQ_LPT1);
                X(IRQ_RTC);
                X(IRQ_FREE1);
                X(IRQ_FREE2);
                X(IRQ_FREE3);
                X(IRQ_PSMOUSE);
                X(IRQ_FPU);
                X(IRQ_ATA1);
                X(IRQ_ATA2);
            }
            warn_unhandled &= ~(1 << irq);
        }
#undef X
    }
    
    if(irq)
        eoi(irq);
}


