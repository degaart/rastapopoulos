#include "idt.h"
#include "vga.h"

#define CODESEG 0x08

static struct idt_entry idt[256];
static struct idtr idtr;
extern uint32_t isr_stub_table[];

int printf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

void idt_set_gate(uint8_t interrupt, uint32_t handler_address,
                  uint16_t selector, uint8_t flags)
{
    idt[interrupt].offset_low = handler_address & 0xFFFF;
    idt[interrupt].selector = selector;
    idt[interrupt].reserved = 0;
    idt[interrupt].flags = flags;
    idt[interrupt].offset_high = (handler_address >> 16) & 0xFFFF;
}

void isr_handler(struct isr_regs* regs)
{
    printf("Exception %lu occurend\n", regs->interrupt_number);
    while (true)
        ;
}

void idt_init(void)
{
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint32_t)(uintptr_t)idt;

    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, isr_stub_table[i], CODESEG,
                     IDT_INT_GATE_32 | IDT_PRESENT);
    }
    idt_load(&idtr);
}

