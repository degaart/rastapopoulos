#include <stdint.h>
#include "util.h"
#include "debug.h"
#include "cxxrt.h"
#include "pmm.h"
#include "vmm.h"
#include "gdt.h"
#include "idt.h"
#include "kmalloc.h"
#include "string.h"
#include "../bootldr/kernel_params.h"
#include "heap.h"
#include "kheap.h"
#include "pic.h"
#include "pit.h"
#include "regs.h"
#include "io.h"
#include "timer.h"

extern "C"
void switch_to_usermode();

extern "C"
void usermode_program();

static void run_tests();

#define SYSCALL_HALT    0x0
#define SYSCALL_WRITE   0x1

static void syscall_write(const char* str) {
    TRACE("<ring3>: %s", str);
}

static void syscall_handler(const isr_regs_t* regs) {
    TRACE("syscall: 0x%X", regs->eax);
    switch(regs->eax) {
        case SYSCALL_HALT:
            halt();
            break;
        case SYSCALL_WRITE:
            syscall_write((const char*)regs->ecx);
            break;
        default:
            PANIC("Unhandled syscall 0x%X", regs->eax);
    }
}

static void gpf_handler(const isr_regs_t* regs) {
    PANIC("General Protection Fault at 0x%X:0x%X", regs->cs, regs->eip);
}

extern "C" void main() {
    static const kernel_params* kparams = (kernel_params*)0x500;

	TRACE("*** RastapopoulOS kernel loaded ***");
    call_ctors();

    TRACE("Initializing kernel heap");
    KHeap::init();
    KHeap::dump();

    TRACE("Initializing GDT");
    GDT::init();
    GDT::clear_iomap(0xE9);     /* Allow access to port e9 for ring3 programs */

    TRACE("Initializing IDT");
    IDT::init();
    IDT::flush();
    IDT::install_handler(13, gpf_handler);

    TRACE("Initializing PIC");
    PIC::init();

    TRACE("Initializing system timer");
    PIT::init();

	TRACE("Initializing PMM");
	PMM::init(kparams->memmap, kparams->memmap_size);
    PMM::dump();
    TRACE("%u pages total (%u bytes)", PMM::pages_total(), PMM::pages_total() * PMM::PAGE_SIZE);

    TRACE("Initializing VMM");
    VMM::init();
    TRACE("%u pages free (%u Kb)", PMM::pages_free(), (PMM::pages_total() * PMM::PAGE_SIZE) / 1024);
    TRACE("Physical memory zones:");
    PMM::dump_zones();

    run_tests();
    halt();
}

static uint32_t _timer1 = 0;
static void timer1(void* args) {
    _timer1++;
    TRACE(" timer1: %u", _timer1);
}


static uint32_t _timer2 = 0;
static void timer2(void* args) {
    _timer2++;
    TRACE("timer2: %u", _timer2);
}

static void timer3(void* args) {
    uint32_t timer1_id = (uint32_t)args;
    TRACE("timer3: unscheduling timer1");
    Timer::unschedule(timer1_id);
}

static void timer5(void* args) {
    TRACE("Hello, I'm timer5. Nice to meet you");
}

static void timer4(void* args) {
    uint32_t timer2_id = (uint32_t)args;
    TRACE("timer4: unscheduling timer2");
    Timer::unschedule(timer2_id);
    Timer::schedule(timer5, nullptr, 2000);
}

static void run_tests() {
    TRACE("Testing Timer");
    sti();

    uint32_t timer1_id = Timer::schedule(timer1, nullptr, 1000);
    uint32_t timer2_id = Timer::schedule(timer2, nullptr, 2250);
    Timer::schedule(timer3, (void*)timer1_id, 5000, false);
    Timer::schedule(timer4, (void*)timer2_id, 10000, false);

    while(true) {
        yield();
    }
}
