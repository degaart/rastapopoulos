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
#include "process.h"

static void test_usermode();

#define SYSCALL_HALT    0x0
#define SYSCALL_WRITE   0x1
#define SYSCALL_YIELD   0x2

static void syscall_yield(const isr_regs_t* regs) {
    uint32_t esp;
    read_esp(esp);

    uint32_t eip = read_eip();

    // TRACE("Yield in %s: ESP = 0x%X, EIP: 0x%X", _current_process->name, esp, eip);
    // BREAKPOINT();
    sti();
    yield();
}

static void syscall_write(const char* str) {
    TRACE("<ring3>: %s", str);
}

static void syscall_handler(const isr_regs_t* regs) {
    // TRACE(
    //     "syscall:\n"
    //     "\teax: 0x%X, ebx: 0x%X\n"
    //     "\tecx: 0x%X, edx: 0x%X\n",
    //     regs->eax, regs->ebx,
    //     regs->ecx, regs->edx
    // );
    uint32_t func = regs->eax;
    uint32_t param0 = regs->ebx;
    uint32_t param1 = regs->ecx;
    uint32_t param2 = regs->edx;

    switch(func) {
        case SYSCALL_HALT:
            halt();
            break;
        case SYSCALL_WRITE:
            syscall_write((const char*)param0);
            break;
        case SYSCALL_YIELD:
            syscall_yield(regs);
            break;
        default:
            PANIC("Unhandled syscall 0x%X", func);
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

    IDT::install_handler(0x80, syscall_handler, true);
    test_usermode();
    halt();
}

static void ring3_syscall(uint32_t function, uint32_t param0 = 0, uint32_t param1 = 0, uint32_t param2 = 0) {
    asm(
        ".intel_syntax noprefix\n"
        "mov eax, [ebp+8]\n"
        "mov ebx, [ebp+12]\n"
        "mov ecx, [ebp+16]\n"
        "mov edx, [ebp+20]\n"
        "int 0x80\n"
        ::: "eax", "ebx", "ecx", "edx", "esi", "edi", "memory"
    );
}

#define SQUELCH (1<<20)
struct process_data {
    char name[32];
    uint32_t counter;
    char chars[5];
    char array[1024];
    uint32_t seed;
    int symbol;
};

static void process_entry() {
    volatile process_data* data = (process_data*)VMM::USERSPACE_START;
    TRACE("%s started (seed: 0x%X)", data->name, data->seed);

    Random rand(data->seed);
    for(unsigned i=0;; i++) {
        ++data->counter;
        if(data->counter == 4)
            data->counter = 0;

        // IO::outb(0xE9, data->chars[data->counter]);
        IO::outb(0xE9, data->symbol);
        uint32_t delay = (1 << (rand.next() % 24));
        for(unsigned j=0; j<SQUELCH; j++)
            data->array[j % sizeof(data->array)] ^= (data->array[j % sizeof(data->array)] ^ i);

        ring3_syscall(SYSCALL_YIELD);
    }
    ring3_syscall(SYSCALL_HALT);
}

static void test_usermode() {
    TRACE("Testing Process manager");
    Process::init();

    Process* p0 = Process::create("Process #0");
    Process* p1 = Process::create("Process #1");

    p0->resume();
}



