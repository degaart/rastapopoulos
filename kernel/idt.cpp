#include "idt.h"
#include "gdt.h"
#include "debug.h"
#include "string.h"
#include "util.h"

// A struct describing an interrupt gate.
struct idt_entry_t {
   uint16_t base_lo;             // The lower 16 bits of the address to jump to when this interrupt fires.
   uint16_t sel;                 // Kernel segment selector.
   uint8_t  always0;             // This must always be zero.
   uint8_t  flags;               // More flags. See documentation.
   uint16_t base_hi;             // The upper 16 bits of the address to jump to.
} __attribute__((packed));

// A struct describing a pointer to an array of interrupt handlers.
// This is in a format suitable for giving to 'lidt'.
struct idt_ptr_t {
   uint16_t limit;
   uint32_t base;                // The address of the first element in our idt_entry_t array.
} __attribute__((packed));

static idt_entry_t          idt_entries[256];
static idt_ptr_t            idt_ptr;
static IDT::isr_handler_t   isr_handlers[256];
extern uint32_t             isr_stub_table[];           /* Yes, this is an array, not a pointer */

void IDT::init() {
    idt_ptr.limit = sizeof(idt_entry_t) * 256 -1;
    idt_ptr.base  = (uint32_t)&idt_entries;

    bzero(isr_handlers, sizeof(isr_handler_t));
    bzero(idt_entries, sizeof(idt_entries));
    for(int i=0; i<256; i++) {
        set_gate(i, isr_stub_table[i], KERNEL_CODE_SEG, IDT_PRESENT|IDT_DPL0|IDT_INT_GATE_32);
    }
}

extern "C" void idt_flush(idt_ptr_t*);
void IDT::flush() {
    idt_flush(&idt_ptr);
}

void IDT::dump() {
    // TRACE("isr_stub_table:");
    // for(int i=0; i<256; i++) {
    //     TRACE("%d %P", i, isr_stub_table[i]);
    // }
}

void IDT::set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt_entries[num].base_lo = base & 0xFFFF;
    idt_entries[num].base_hi = (base >> 16) & 0xFFFF;

    idt_entries[num].sel     = sel;
    idt_entries[num].always0 = 0;
    // We must uncomment the OR below when we get to using user-mode.
    // It sets the interrupt gate's privilege level to 3.
    idt_entries[num].flags   = flags | IDT_DPL3 ;
}

extern "C" void isr_handler(isr_regs_t regs) {
    if(isr_handlers[regs.int_no]) {
        isr_handlers[regs.int_no](&regs);
    } else {
        TRACE(
            "Unhandled interrupt: %d\n"
            "\tds:  0x%X\n"
            "\teax: 0x%X ebx: 0x%X ecx: 0x%X edx: 0x%X\n"
            "\tesi: 0x%X edi: 0x%X\n"
            "\terr: 0x%X\n"
            "\tcs:  0x%X eip: 0x%X eflags: 0x%X\n"
            "\tss:  0x%X esp: 0x%X\n",
            regs.int_no,
            regs.ds,
            regs.eax, regs.ebx, regs.ecx, regs.edx,
            regs.esi, regs.edi,
            regs.err_code,
            regs.cs, regs.eip, regs.eflags, 
            regs.ss, regs.esp
        );
        halt();
    }
}

void IDT::install_handler(int num, isr_handler_t handler) {
    pushf();
    cli();
    isr_handlers[num] = handler;
    popf();
}
