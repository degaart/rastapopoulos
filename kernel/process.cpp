#include "process.h"
#include "string.h"
#include "util.h"
#include "debug.h"
#include "vmm.h"
#include "pmm.h"
#include "gdt.h"
#include "timer.h"
#include "initrd.h"

#define EXE_MAGIC "Rasta Executable"
#define EPHEMERAL_PORT_START 65536

Process::ProcessList_t Process::_processes;
Process* Process::_current_process = nullptr;
uint32_t Process::_current_pid = 0;
uint32_t Process::_current_ephemeral_port = EPHEMERAL_PORT_START;

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

    _current_ring       = RING3;
    _kernel_esp         = (uint32_t)(_kernel_stack + sizeof(_kernel_stack) - 1);
    _regs.esp           = USER_STACK_END;
    _regs.eax           = pid;
    
    read_eflags(_regs.eflags);
    _regs.eflags        |= EFLAGS_IF;
    _regs.eip           = PROCESS_ENTRY;

    _user_stack         = USER_STACK_END - (VMM::PAGE_SIZE*4) + 1;  /* gives a nice page-aligned stack */
    assert((_user_stack % VMM::PAGE_SIZE) == 0);
    _workingset_size    = 0;

    _pagedir = VMM::create_pagedir();
    VMM::switch_pagedir(_pagedir);
    VMM::alloc(_user_stack, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE|VMM::PAGE_USER);
    VMM::alloc(_user_stack + VMM::PAGE_SIZE, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE|VMM::PAGE_USER);
    VMM::alloc(_user_stack + (VMM::PAGE_SIZE*2), VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE|VMM::PAGE_USER);
    VMM::alloc(_user_stack + (VMM::PAGE_SIZE*3), VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE|VMM::PAGE_USER);

    // VMM::alloc(PROCESS_ENTRY, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE|VMM::PAGE_USER);
    // memcpy((void*)PROCESS_ENTRY, ___hello_obj_hello_bin, ___hello_obj_hello_bin_size);
}

Process::~Process() {
    /*
        - Free any allocated Page-frames, keeping shared pages
        - Switch to another known-good Pagedir before destroying this process's pagedir

        For now: we assume user pages can't be shared
        Note: Pagedir's destructor does not free any pageframes, we must do it ourselves
    */
    TRACE("Destroying %s (pid %u)", _name, _pid);
    for(auto i = _ports.iterator(); i.valid(); i.next()) {
        delete *i;
    }

    for(uint32_t va = PROCESS_ENTRY; va < PROCESS_ENTRY + _workingset_size; va += VMM::PAGE_SIZE) {
        _pagedir->free(va);
    }
    for(uint32_t va = _user_stack; va < USER_STACK_END; va += VMM::PAGE_SIZE) {
        _pagedir->free(va);
    }

    VMM::free_pagedir(_pagedir);
}

Process* Process::create(const char* name) {
    Process* proc = new Process(++_current_pid, name);
    _processes.append(proc);
    return proc;
}

void Process::switch_process(Process* process) {
    assert(process->_workingset_size >= VMM::PAGE_SIZE);
    assert(process->_regs.eflags & EFLAGS_IF);

    assert(_processes.contains(process));
    _current_process = process;
    
    GDT::set_kernel_stack((void*)process->_kernel_esp);
    VMM::switch_pagedir(process->_pagedir);

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
    assert(_current_process != nullptr);

    Ring current_ring = (regs->cs & 0x3) ? RING3 : RING0;
    // TRACE("%s preempted in ring %u", _current_process->name, (unsigned)current_ring);

    if(current_ring == RING3) {
        /* Interrupt originated from ring3 */
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

    /* Switch to next process */
    switch_process(next_process());
    PANIC("Should not happen");
}

void Process::init() {
    Timer::schedule(resume_next_process, nullptr, 250);
}

void Process::exit_current_process() {
    Process* proc = _current_process;
    _current_process = next_process();
    if(_current_process == proc) {
        PANIC("No more processes to run!");
    }

    _processes.remove_val(proc);

    VMM::switch_pagedir(_current_process->_pagedir);
    delete(proc);
    proc = nullptr;

    switch_process(_current_process);
    PANIC("Should not happen");
}

Process* Process::next_process() {
    assert(_processes.size() != 0);

    /* If no current process, return first process */
    if(_current_process == nullptr)
        return *_processes.iterator();
    
    /* Else return process immediately following current process */
    ProcessList_t::Iterator it = _processes.find(_current_process);
    assert(it.valid());
    it.next();
    if(it.valid())
        return *it;
    
    /* Else return first process */
    return *_processes.iterator();
}

void Process::load_image(const char* filename) {
    if(_workingset_size != 0) {
        PANIC("Process image alread loaded");
    }

    Initrd::File* file = Initrd::get().open(filename);
    if(!file) {
        PANIC("File not found in initrd: %s", filename);
    }
    if(memcmp((uint8_t*)file->data() + 2, EXE_MAGIC, sizeof(EXE_MAGIC))) {
        PANIC("Invalid executable (bad magic): %s", filename);
    }

    VMM::switch_pagedir(_pagedir);
    for(uint32_t page = PROCESS_ENTRY; page < PROCESS_ENTRY + file->size(); page += VMM::PAGE_SIZE) {
        VMM::alloc(page, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE|VMM::PAGE_USER);
        _workingset_size += VMM::PAGE_SIZE;
        
        int read_bytes = file->read((uint8_t*)page, VMM::PAGE_SIZE);
        if(read_bytes == 0 || read_bytes < VMM::PAGE_SIZE) {
            break;
        }
    }
    delete file;
}

Process* Process::current_process() {
    return _current_process;
}

const char* Process::name() {
    return _name;
}

uint32_t Process::open_port(uint32_t port_number) {
    if(port_number != INVALID_PORT) {
        /* Check if another process hasn't opened that port */
        if(process_for_port(port_number) != nullptr) {
            return INVALID_PORT;
        }
    } else if(port_number >= EPHEMERAL_PORT_START) {
        return INVALID_PORT;
    } else {
        port_number = ephemeral_port_number();
    }

    Port* port = new Port();
    port->number = port_number;
    _ports.append(port);
    return port_number;
}

bool Process::close_port(uint32_t port_number) {
    assert(port_number != INVALID_PORT);

    /* Check if port is really opened by current process */
    auto port = port_iterator(port_number);
    if(!port.valid())
        PANIC("Process %s (PID %d) tried to close invalid port %u", _name, _pid, port_number);

    Port* p = *port;
    _ports.remove(port);
    delete p;
    return true;
}

uint32_t Process::ephemeral_port_number() {
    if(_current_ephemeral_port == UINT32_MAX)
        PANIC("Ephemeral ports number exhaustion");

    return _current_ephemeral_port++;
}

Port* Process::get_port(uint32_t port_number) {
    auto i = port_iterator(port_number);
    if(i.valid())
        return *i;
    return nullptr;
}

Process* Process::process_for_port(uint32_t port_number) {
    for(auto i = _processes.iterator(); i.valid(); i.next()) {
        Port* port = (*i)->get_port(port_number);
        if(port)
            return *i;
    }
    return nullptr;
}

LinkedList<Port*>::Iterator Process::port_iterator(uint32_t port_number) {
    for(auto i = _ports.iterator(); i.valid(); i.next()) {
        if((*i)->number == port_number)
            return i;
    }
    return LinkedList<Port*>::Iterator();
}

void Process::check_readable_block(const void* buffer, size_t size) {
    assert(this == _current_process);

    uint32_t first_unreadable, first_invalid;
    if(!_pagedir->check_readable_block(buffer, size, &first_unreadable, &first_invalid)) {
        if(first_unreadable) {
            TRACE("Process %s: Access violation (unreadable page) at page 0x%X. Terminated", _name, first_unreadable);
            exit_current_process();
        } else {
            TRACE("Process %s: Unmapped page at 0x%X. Terminated", _name, first_invalid);
            exit_current_process();
        }
    }
}

void Process::check_writable_block(const void* buffer, size_t size) {
    assert(this == _current_process);

    uint32_t first_readonly, first_invalid;
    if(!_pagedir->check_writable_block(buffer, size, &first_readonly, &first_invalid)) {
        if(first_readonly) {
            TRACE("Process %s: Access violation (read-only) at page at 0x%X. Terminated", _name, first_readonly);
            exit_current_process();
        } else {
            TRACE("Process %s: Unmapped page at 0x%X. Terminated", _name, first_invalid);
            exit_current_process();
        }
    }
}


