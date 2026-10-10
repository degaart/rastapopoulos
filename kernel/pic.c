#include "pic.h"
#include "kernel.h"
#include "trace.h"
#include "util.h"

#define PIC1            0x20 /* IO base address for master PIC */
#define PIC2            0xA0 /* IO base address for slave PIC */
#define PIC1_COMMAND    PIC1
#define PIC1_DATA       (PIC1 + 1)
#define PIC2_COMMAND    PIC2
#define PIC2_DATA       (PIC2 + 1)
#define PIC_EOI         0x20 /* End-of-interrupt command code */
#define ICW1_ICW4       0x01 /* Indicates that ICW4 will be present */
#define ICW1_SINGLE     0x02 /* Single (cascade) mode */
#define ICW1_INTERVAL4  0x04 /* Call address interval 4 (8) */
#define ICW1_LEVEL      0x08 /* Level triggered (edge) mode */
#define ICW1_INIT       0x10 /* Initialization - required! */
#define ICW4_8086       0x01 /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO       0x02 /* Auto (normal) EOI */
#define ICW4_BUF_SLAVE  0x08 /* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C /* Buffered mode/master */
#define ICW4_SFNM       0x10 /* Special fully nested (not) */
#define CASCADE_IRQ     2
#define PIC_READ_IRR    0x0a /* OCW3 irq ready next CMD read */
#define PIC_READ_ISR    0x0b /* OCW3 irq service next CMD read */

static irq_handler_t handlers[16];

static void isr_handler(int number, struct isr_regs* regs)
{
    int irq = number - 32;
    if (irq < 0 || irq >= ARRAY_SIZE(handlers))
        trace("Invalid IRQ: %d", number);

    if (irq == 7) {
        if (!(pic_get_isr() & (1 << 7))) {
            trace("WARNING: Spurious irq7");
            return;
        }
    } else if (irq == 15) {
        if (!(pic_get_isr() & (1 << 15))) {
            trace("WARNING: Spurious irq15");
            pic_eoi(2);
            return;
        }
    }

    if (!handlers[irq]) {
        trace("WARNING: No handler for IRQ %d", irq);
        return;
    }

    handlers[irq](irq, regs);
}

void pic_init(void)
{
    outb(
        PIC1_COMMAND,
        ICW1_INIT |
            ICW1_ICW4); // starts the initialization sequence (in cascade mode)
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC1_DATA, 32); // ICW2: Master PIC vector offset
    io_wait();
    outb(PIC2_DATA, 32 + 8); // ICW2: Slave PIC vector offset
    io_wait();
    outb(PIC1_DATA, 1 << CASCADE_IRQ); // ICW3: tell Master PIC that there is a
                                       // slave PIC at IRQ2
    io_wait();
    outb(PIC2_DATA, CASCADE_IRQ); // ICW3: tell Slave PIC its cascade identity
    io_wait();

    outb(PIC1_DATA,
         ICW4_8086); // ICW4: have the PICs use 8086 mode (and not 8080 mode)
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    // Unmask both PICs.
    outb(PIC1_DATA, 0);
    outb(PIC2_DATA, 0);

    for (int i = 32; i < 32 + 16; i++) {
        idt_set_handler(i, isr_handler);
    }
}

void pic_eoi(int irq)
{
    if (irq < 0 || irq > 15)
        panic("Invalid IRQ: %d", irq);

    if ((pic_get_isr() & (1 << irq)) == 0) {
        panic("IRQ %d is not being serviced", irq);
    }

    if (irq >= 0)
        outb(PIC2_COMMAND, PIC_EOI);
    outb(PIC1_COMMAND, PIC_EOI);
}

irq_handler_t pic_set_irq_handler(int num, irq_handler_t handler)
{
    if (num < 0 || num > 15)
        panic("Invalid IRQ: %d", num);

    irq_handler_t result = handlers[num];
    handlers[num] = handler;
    return result;
}

void pic_disable(void)
{
    outb(PIC1_DATA, 0xff);
    outb(PIC2_DATA, 0xff);
}

void pic_mask_irq(int irq)
{
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) | (1 << irq);
    outb(port, value);
}

void pic_unmask_irq(int irq)
{
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}

static uint16_t get_irq_reg(int ocw3)
{
    /* OCW3 to PIC CMD to get the register values.  PIC2 is chained, and
     * represents IRQs 8-15.  PIC1 is IRQs 0-7, with 2 being the chain */
    outb(PIC1_COMMAND, ocw3);
    outb(PIC2_COMMAND, ocw3);
    return (inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND);
}

uint16_t pic_get_irr(void)
{
    return get_irq_reg(PIC_READ_IRR);
}

uint16_t pic_get_isr(void)
{
    return get_irq_reg(PIC_READ_ISR);
}

