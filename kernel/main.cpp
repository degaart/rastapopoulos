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
#include "hello.h"

extern "C"
void switch_to_usermode(
    uint32_t esp, uint32_t eflags, uint32_t eip,
    uint32_t edi, uint32_t esi,
    uint32_t edx, uint32_t ecx, uint32_t ebx, uint32_t eax,
    uint32_t ebp
);

extern "C"
void resume_from_interrupt(
    uint32_t esp, uint32_t eflags, uint32_t eip,
    uint32_t edi, uint32_t esi,
    uint32_t edx, uint32_t ecx, uint32_t ebx, uint32_t eax,
    uint32_t ebp
);

static void test_usermode();

enum Ring {
    RING0,
    RING1,
    RING2,
    RING3
};

struct process_t {
    char name[32];
    uint8_t* kernel_stack;
    uint8_t* user_stack;
    Pagedir* pagedir;
    Ring current_ring;
    
    uint32_t kernel_esp;

    uint32_t esp;
    uint32_t eflags;
    uint32_t eip;
    uint32_t edi;
    uint32_t esi;
    uint32_t edx;
    uint32_t ecx;
    uint32_t ebx;
    uint32_t eax;
    uint32_t ebp;
};
process_t _process0;
process_t _process1;
process_t* _current_process = nullptr;

#define SYSCALL_HALT    0x0
#define SYSCALL_WRITE   0x1
#define SYSCALL_YIELD   0x2

static void switch_process(process_t* process) {
    assert(process->eflags & EFLAGS_IF);

    // if(_current_process) {
    //     if(_current_process->current_ring == RING0) {
    //         if(process->current_ring == RING0)
    //             Debug::write_string(" RING0 -> RING0 ");
    //         else
    //             Debug::write_string(" RING0 -> RING3 ");
    //     } else {
    //         if(process->current_ring == RING0)
    //             Debug::write_string(" RING3 -> RING0 ");
    //         else
    //             Debug::write_string(" RING0 -> RING3 ");
    //     }
    // }

    _current_process = process;
    GDT::set_kernel_stack((void*)process->kernel_esp);
    VMM::switch_pagedir(_current_process->pagedir);

    TRACE("Resuming %s in ring %u", process->name, process->current_ring);
    if(process->current_ring == RING3) {
        switch_to_usermode(
            process->esp, process->eflags, process->eip,
            process->edi, process->esi,
            process->edx, process->ecx, process->ebx, process->eax,
            process->ebp
        );
    } else {
        // TRACE("ESP: 0x%X, EIP: 0x%X", process->esp, process->eip);
        resume_from_interrupt(
            process->esp, process->eflags, process->eip,
            process->edi, process->esi,
            process->edx, process->ecx, process->ebx, process->eax,
            process->ebp  
        );
    }
}

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

    IDT::install_handler(0x80, syscall_handler);
    test_usermode();
    halt();
}

static void scheduler_timer(void* args, const isr_regs_t* regs) {
    Ring current_ring = (regs->cs & 0x3) ? RING3 : RING0;
    TRACE("%s preempted in ring %u", _current_process->name, (unsigned)current_ring);

    if(current_ring == RING3) {
        /* Interrupt originated from ring3 */
        /* Magic: 0x1C */
        _current_process->current_ring = current_ring;
        _current_process->kernel_esp = (uint32_t)GDT::get_kernel_stack();
        _current_process->esp = regs->useresp;
        _current_process->eflags = regs->eflags;
        _current_process->eip = regs->eip;
        _current_process->edi = regs->edi;
        _current_process->esi = regs->esi;
        _current_process->edx = regs->edx;
        _current_process->ecx = regs->ecx;
        _current_process->ebx = regs->ebx;
        _current_process->eax = regs->eax;
        _current_process->ebp = regs->ebp;
    } else {
        /* Interrupt originated from ring0 */
        _current_process->current_ring = current_ring;
        _current_process->kernel_esp = (uint32_t)GDT::get_kernel_stack();        /* Restored on kernel process switch in switch_process() */
        _current_process->esp = regs->esp + 0x14;       /* State of ESP before the pusha in isr_stub */
        _current_process->eflags = regs->eflags;
        _current_process->eip = regs->eip;
        _current_process->edi = regs->edi;
        _current_process->esi = regs->esi;
        _current_process->edx = regs->edx;
        _current_process->ecx = regs->ecx;
        _current_process->ebx = regs->ebx;
        _current_process->eax = regs->eax;
        _current_process->ebp = regs->ebp;
    }

    /*
        resume next process
        which, interrestingly, causes a kernel stack leak, as we aren't returning from this function
    */
    process_t* next_process = (_current_process == &_process0 ? &_process1 : &_process0);
    switch_process(next_process);
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
    TRACE("Testing User-mode");

    uint32_t eflags;
    read_eflags(eflags);
    eflags |= EFLAGS_IF;

    /*
        Address-space layout:
            Ring3:
            0x403000 - 0x403FFF: invalid page
            0x402000 - 0x402FFF: user stack
            0x401000 - 0x401FFF: invalid page
            0x400000 - 0x400FFF: user data

            Ring0:
            0x3FE000 - 0x3FFFFF: invalid page
            0x3FD000 - 0x3FDFFF: kernel stack 1
            0x3FC000 - 0x3FCFFF: invalid page
            0x3FB000 - 0x3FBFFF: kernel stack 2
            0x3FA000 - 0x3FAFFF: invalid page
    */
    VMM::map(0x3FD000, PMM::alloc(), VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE);
    VMM::map(0x3FB000, PMM::alloc(), VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE);

    bzero(&_process0, sizeof(_process0));
    strcpy(_process0.name, "Process 0");
    _process0.kernel_stack = (uint8_t*)0x3FDFFF;
    _process0.user_stack = (uint8_t*)0x402FFF;
    _process0.pagedir = VMM::create_pagedir();
    _process0.esp = (uint32_t)_process0.user_stack;
    _process0.eflags = eflags;
    _process0.eip = 0x400000;
    _process0.kernel_esp = (uint32_t) _process0.kernel_stack;
    _process0.current_ring = RING3;
    VMM::switch_pagedir(_process0.pagedir);
    VMM::map(0x400000, PMM::alloc(), VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE);
    VMM::map(0x402000, PMM::alloc(), VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE);
    memcpy((void*)0x400000, ___hello_obj_hello_bin, ___hello_obj_hello_bin_size);

    bzero(&_process1, sizeof(_process1));
    strcpy(_process1.name, "Process 1");
    _process1.kernel_stack = (uint8_t*)0x3FBFFF;
    _process1.user_stack = (uint8_t*)0x402FFF;
    _process1.pagedir = VMM::create_pagedir();
    _process1.esp = (uint32_t)_process1.user_stack;
    _process1.eflags = eflags;
    _process1.eip = 0x400000;
    _process1.kernel_esp = (uint32_t) _process1.kernel_stack;
    _process1.current_ring = RING3;
    VMM::switch_pagedir(_process1.pagedir);
    VMM::map(0x400000, PMM::alloc(), VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE);
    VMM::map(0x402000, PMM::alloc(), VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE);
    memcpy((void*)0x400000, ___hello_obj_hello_bin, ___hello_obj_hello_bin_size);

    Timer::schedule(scheduler_timer, nullptr, 250);
    switch_process(&_process0);
}


