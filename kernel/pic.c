/*
 * 15 IRQs available: 8 IRQ per 8259 PIT
 * IRQ2 is for cascading
 * PICs will raise an interrupt if the interrupt is not masked and there is
 * no pending interrupt.
 * Each PIT has a command and a data port.
 * PIC vector offsets must be divisible by 8.
 * Must remap: Master PIC to 0x20, slave to 0x28.
 *
 * Registers:
 *  ISR: IRQs sent to CPU
 *  IRR: IRQs which have been raised
 * PICs end ints from the IRR to the CPU, then they are marked in the ISR
 *
 * Spurious IRQs: IRQ7 and IRQ15. ISR flag for int is not set. If IRQ7: do not
 *  send EOI. If IRQ15: send EOI to master PIC only
 *
 * IRQ0   timer (55ms intervals, 18.2 per second)
 * IRQ1   keyboard service required
 * IRQ2   slave 8259 or EGA/VGA vertical retrace
 * IRQ8   real time clock  (AT,XT286,PS50+)
 * IRQ9   software redirected to IRQ2  (AT,XT286,PS50+)
 * IRQ10  reserved  (AT,XT286,PS50+)
 * IRQ11  reserved  (AT,XT286,PS50+)
 * IRQ12  mouse interrupt  (PS50+)
 * IRQ13  numeric coprocessor error  (AT,XT286,PS50+)
 * IRQ14  fixed disk controller (AT,XT286,PS50+)
 * IRQ15  reserved  (AT,XT286,PS50+)
 * IRQ3   COM2 or COM4 service required, (COM3-COM8 on MCA PS/2)
 * IRQ4   COM1 or COM3 service required
 * IRQ5   fixed disk or data request from LPT2
 * IRQ6   floppy disk service required
 * IRQ7   data request from LPT1 (unreliable on IBM mono)
 *  */
#include "idt.h"
#include "pic.h"
#include "util.h"
#include <debug.h>
#include <io.h>

#define PIC1_COMMAND    0x20
#define PIC1_DATA       (PIC1_COMMAND+1)
#define PIC2_COMMAND    0xA0
#define PIC2_DATA       (PIC2_COMMAND+1)
#define PIC_EOI         0x20

#define PIC1_OFFSET     0x20
#define PIC2_OFFSET     0x28

#define ICW1_ICW4	    0x01		/* Indicates that ICW4 will be present */
#define ICW1_SINGLE	    0x02		/* Single (cascade) mode */
#define ICW1_INTERVAL4	0x04		/* Call address interval 4 (8) */
#define ICW1_LEVEL	    0x08		/* Level triggered (edge) mode */
#define ICW1_INIT	    0x10		/* Initialization - required! */
 
#define ICW4_8086	    0x01		/* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO	    0x02		/* Auto (normal) EOI */
#define ICW4_BUF_SLAVE	0x08		/* Buffered mode/slave */
#define ICW4_BUF_MASTER	0x0C		/* Buffered mode/master */
#define ICW4_SFNM	    0x10		/* Special fully nested (not) */

#define PIC_READ_IRR    0x0a        /* OCW3 irq ready next CMD read */
#define PIC_READ_ISR    0x0b        /* OCW3 irq service next CMD read */

static irq_handler_t irq_handlers[16];

void pic_eoi(unsigned irq)
{
    assert(irq >= 0);
    assert(irq < 16);

    if(irq >= 8)
        outb(PIC2_COMMAND, PIC_EOI);
    outb(PIC1_COMMAND, PIC_EOI);
}

static void pic_remap()
{
    uint8_t a1 = inb(PIC1_DATA);                /* save masks */
    uint8_t a2 = inb(PIC2_DATA);

    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);  /* starts the initialization */
    io_wait();                                  /* sequence (cascade mode) */
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC1_DATA, PIC1_OFFSET);          /* ICW2: Master PIC vector offset */
    io_wait();
    outb(PIC2_DATA, PIC2_OFFSET);          /* ICW2: Slave PIC vector offset */
    io_wait();
    outb(PIC1_DATA, 4);             /* ICW3: tell Master PIC that there is a */
    io_wait();                      /* slave PIC at IRQ2 (0000 0100) */
    outb(PIC2_DATA, 2);             /* ICW3: tell Slave PIC its cascade */
    io_wait();                      /* identity (0000 0010) */

    outb(PIC1_DATA, ICW4_8086);         /* ICW4: have the PICs use 8086 mode */
    io_wait();                                       /*  (and not 8080 mode) */
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    outb(PIC1_DATA, a1);                        /* restore saved masks. */
    outb(PIC2_DATA, a2);
}

void pic_mask(unsigned line)
{
    assert(line > 0);
    assert(line < 16);

    uint16_t port;
    if(line < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        line -= 8;
    }

    uint8_t value = inb(port) | (1 << line);
    outb(port, value);
}

void pic_unmask(unsigned line)
{
    assert(line > 0);
    assert(line < 16);

    uint16_t port;
    if(line < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        line -= 8;
    }
    uint8_t value = inb(port) & ~(1 << line);
    outb(port, value);
}

static uint16_t pic_irq_reg(unsigned ocw3)
{
    outb(PIC1_COMMAND, ocw3);
    outb(PIC2_COMMAND, ocw3);
    return (inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND);
}

uint16_t pic_irr()
{
    return pic_irq_reg(PIC_READ_IRR);
}

uint16_t pic_isr()
{
    return pic_irq_reg(PIC_READ_IRR);
}

static void isr_handler(struct isr_regs* regs)
{
    assert(regs->int_no >= 0x20);
    assert(regs->int_no - 0x20 < sizeof(irq_handlers)/sizeof(irq_handlers[0]));
    unsigned irq = regs->int_no - 0x20;

    /* handle spurious IRQs */
    if(irq == 7) {
        if(!(pic_isr() & (1 << irq))) {
            TRACE("Spurious IRQ7 detected");
        }
    } else if(irq == 15) {
        if(!(pic_isr() & (1 << irq))) {
            TRACE("Spurious IRQ15 detected");
            outb(PIC1_COMMAND, PIC_EOI);
        }
    }

    pic_eoi(irq);
    if(irq_handlers[irq]) {
        irq_handlers[irq]();
    } else {
        TRACE("Unhandled IRQ%d", irq);
    }
}

void pic_set_irq_handler(unsigned irq, irq_handler_t handler)
{
    assert(irq < sizeof(irq_handlers)/sizeof(irq_handlers[0]));
    irq_handlers[irq] = handler;
}

void pic_init()
{
    pic_remap();
    for(unsigned i = 0x20; i < 0x20 + 16; i++) {
        idt_add_handler(i, isr_handler, 0);
    }
}

