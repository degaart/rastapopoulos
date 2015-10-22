#include "process.h"
#include "string.h"
#include "util.h"
#include "debug.h"
#include "vmm.h"
#include "gdt.h"
#include "timer.h"
#include "hello.h"

Process* Process::_processes[100];            /* FUCK THE POLICE! */
uint32_t Process::_process_count = 0;
uint32_t Process::_current_pid = 0;
Process* Process::_current_process = nullptr;

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

Process::Process(uint32_t pid, const char* name)
: _pid(pid) {
    strlcpy(_name, name, sizeof(_name));
    bzero(&_regs, sizeof(_regs));

    _current_ring   = RING3;
    _kernel_esp     = (uint32_t)(_kernel_stack + sizeof(_kernel_stack) - 1);
    _regs.esp       = USER_STACK_END;
    _regs.eax       = pid;
    
    read_eflags(_regs.eflags);
    _regs.eflags    |= EFLAGS_IF;
    _regs.eip       = PROCESS_ENTRY;

    _user_stack     = (uint8_t*)USER_STACK_END - 4095;  /* gives a nice page-aligned stack */
    assert(((uint32_t)_user_stack % 4096) == 0);

    _pagedir = VMM::create_pagedir();
    VMM::switch_pagedir(_pagedir);
    VMM::alloc(PROCESS_ENTRY, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE|VMM::PAGE_USER);
    VMM::alloc(_user_stack, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE|VMM::PAGE_USER);
    memcpy((void*)PROCESS_ENTRY, ___hello_obj_hello_bin, ___hello_obj_hello_bin_size);
}

Process::~Process() {
    /*
        - Free any allocated Page-frames, keeping shared pages
        - Switch to another known-good Pagedir before detroying this process's pagedir
    */
    PANIC("Not implemented yet");
}

void Process::resume() {
    switch_process(this);
}

Process* Process::create(const char* name) {
    assert(_process_count != 10);

    Process* proc = new Process(++_current_pid, name);
    _processes[_process_count] = proc;
    _process_count++;

    return proc;
}

void Process::switch_process(Process* process) {
    assert(process->_regs.eflags & EFLAGS_IF);

    _current_process = process;
    GDT::set_kernel_stack((void*)process->_kernel_esp);
    VMM::switch_pagedir(_current_process->_pagedir);

    if(process->_current_ring == RING3) {
        switch_to_usermode(
            process->_regs.esp, process->_regs.eflags, process->_regs.eip,
            process->_regs.edi, process->_regs.esi,
            process->_regs.edx, process->_regs.ecx, process->_regs.ebx, process->_regs.eax,
            process->_regs.ebp
        );
    } else {
        // TRACE("ESP: 0x%X, EIP: 0x%X", process->esp, process->eip);
        resume_from_interrupt(
            process->_regs.esp, process->_regs.eflags, process->_regs.eip,
            process->_regs.edi, process->_regs.esi,
            process->_regs.edx, process->_regs.ecx, process->_regs.ebx, process->_regs.eax,
            process->_regs.ebp
        );
    }
}

void Process::resume_next_process(void* args, const isr_regs_t* regs) {
    Ring current_ring = (regs->cs & 0x3) ? RING3 : RING0;
    // TRACE("%s preempted in ring %u", _current_process->name, (unsigned)current_ring);

    if(current_ring == RING3) {
        /* Interrupt originated from ring3 */
        /* Magic: 0x1C */
        _current_process->_current_ring = current_ring;
        _current_process->_kernel_esp = (uint32_t)GDT::get_kernel_stack();
        _current_process->_regs.esp = regs->useresp;
        _current_process->_regs.eflags = regs->eflags;
        _current_process->_regs.eip = regs->eip;
        _current_process->_regs.edi = regs->edi;
        _current_process->_regs.esi = regs->esi;
        _current_process->_regs.edx = regs->edx;
        _current_process->_regs.ecx = regs->ecx;
        _current_process->_regs.ebx = regs->ebx;
        _current_process->_regs.eax = regs->eax;
        _current_process->_regs.ebp = regs->ebp;
    } else {
        /* Interrupt originated from ring0 */
        _current_process->_current_ring = current_ring;
        _current_process->_kernel_esp = (uint32_t)GDT::get_kernel_stack();        /* Restored on kernel process switch in switch_process() */
        _current_process->_regs.esp = regs->esp + 0x14;       /* State of ESP before the pusha in isr_stub */
        _current_process->_regs.eflags = regs->eflags;
        _current_process->_regs.eip = regs->eip;
        _current_process->_regs.edi = regs->edi;
        _current_process->_regs.esi = regs->esi;
        _current_process->_regs.edx = regs->edx;
        _current_process->_regs.ecx = regs->ecx;
        _current_process->_regs.ebx = regs->ebx;
        _current_process->_regs.eax = regs->eax;
        _current_process->_regs.ebp = regs->ebp;
    }

    for(unsigned i = 0; i<_process_count; i++) {
        if(_processes[i] == _current_process) {
            unsigned next_process_idx = (i + 1) % _process_count;
            switch_process(_processes[next_process_idx]);
        }
    }

    PANIC("Should not happen");
}

void Process::init() {
    bzero(_processes, sizeof(_processes));
    Timer::schedule(resume_next_process, nullptr, 250);
}


