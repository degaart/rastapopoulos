#include "idt.h"
#include "kernel.h"
#include "vga.h"
#include <gdt.h>
#include <stdio.h>

static struct idt_entry idt[256];
static struct idtr idtr;
extern uint32_t isr_stub_table[];
static idt_handler_t handlers[256];

static void idt_set_gate(uint8_t interrupt, uint32_t handler_address,
                         uint16_t selector, uint8_t flags)
{
    idt[interrupt].offset_low = handler_address & 0xFFFF;
    idt[interrupt].selector = selector;
    idt[interrupt].reserved = 0;
    idt[interrupt].flags = flags;
    idt[interrupt].offset_high = (handler_address >> 16) & 0xFFFF;
}

void idt_isr_handler(struct isr_regs* regs)
{
    if (handlers[regs->interrupt_number]) {
        handlers[regs->interrupt_number](regs->interrupt_number, regs);
        return;
    }

    uint32_t ss = read_ss();
    uint32_t cr0 = read_cr0();
    uint32_t cr2 = read_cr2();
    panic("Unhandled interrupt:\n"
          "    vector: 0x%lx\n"
          "    error_code: 0x%lx\n"
          "    eip: 0x%lx cs: 0x%lx\n"
          "    eflags: 0x%lx\n"
          "    esp: 0x%lx ss: 0x%lx\n"
          "    cr0: 0x%lx cr2: 0x%lx\n"
          "    eax: 0x%lx ebx: 0x%lx ecx: 0x%lx edx: 0x%lx\n"
          "    esi: 0x%lx edi: 0x%lx ebp: 0x%lx",
          regs->interrupt_number, regs->error_code, regs->eip, regs->cs,
          regs->eflags, regs->esp, ss, cr0, cr2, regs->eax, regs->ebx,
          regs->ecx, regs->edx, regs->esi, regs->edi, regs->ebp);
}

void idt_init(void)
{
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint32_t)(uintptr_t)idt;

    for (int i = 0; i < 256; i++) {
        unsigned flags = IDT_INT_GATE_32 | IDT_PRESENT;
        if (i == 3 ||
            i == 3) /* int 3 and int 4 need to be callable from ring3 */
            flags |= IDT_DPL3;
        idt_set_gate(i, isr_stub_table[i], CODESEG, flags);
    }
    idt_load(&idtr);
}

idt_handler_t idt_set_handler(int vector, idt_handler_t handler, unsigned dpl)
{
    if (vector < 0 || vector > 255)
        panic("Invalid interrupt vector %d", vector);
    else if (dpl > 3)
        panic("Invalid interrupt vector DPL: %u", dpl);
    idt_handler_t result = handlers[vector];
    handlers[vector] = handler;
    idt_set_gate(vector, isr_stub_table[vector], CODESEG,
                 IDT_INT_GATE_32 | IDT_PRESENT | (dpl << 5));
    return result;
}

