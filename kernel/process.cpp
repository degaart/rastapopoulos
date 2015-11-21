#include "process.h"
#include "string.h"
#include "util.h"
#include "debug.h"
#include "vmm.h"
#include "pmm.h"
#include "gdt.h"
#include "idt.h"
#include "timer.h"
#include "initrd.h"
#include "elf.h"
#include "kernel_task.h"
#include "context.h"

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
    uint32_t ebp,
    uint32_t cr3
);

extern "C"
void resume_from_interrupt(
    uint32_t esp, uint32_t eflags, uint32_t eip,
    uint32_t edi, uint32_t esi,
    uint32_t edx, uint32_t ecx, uint32_t ebx, uint32_t eax,
    uint32_t ebp
);

Process::Process(uint32_t pid)
: _pid(pid) {
    bzero(_name, sizeof(_name));
    _pagedir        = nullptr;
    _kernel_stack   = 0;
    _user_stack     = 0;
    _current_ring   = RING0;
    _kernel_esp     = 0;
    bzero(&_regs, sizeof(_regs));

#if 0
    assert((KERNEL_STACK_START % VMM::PAGE_SIZE) == 0);
    _current_ring       = RING3;
    _kernel_esp         = KERNEL_STACK_END;
    _regs.esp           = USER_STACK_END;
    _regs.eax           = pid;
    
    read_eflags(_regs.eflags);
    _regs.eflags        |= EFLAGS_IF;
    _regs.eip           = 0;

    _user_stack         = USER_STACK_END - VMM::PAGE_SIZE + 1;  /* gives a nice page-aligned stack */
    assert((_user_stack % VMM::PAGE_SIZE) == 0);
    _workingset_size    = 0;

    if(alloc_stack) {
        _pagedir = VMM::create_pagedir();
        _pagedir->alloc(_user_stack, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE|VMM::PAGE_USER);
        _pagedir->alloc(KERNEL_STACK_START, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE);
        _pagedir->alloc(KERNEL_STACK_START + VMM::PAGE_SIZE, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE);
    }
#endif
}

Process::~Process() {
    TRACE("Destroying %s (pid %u)", _name, _pid);
    for(auto i = _ports.iterator(); i.valid(); i.next()) {
        delete *i;
    }

    /*
        - Switch to another known-good Pagedir before destroying this process's pagedir
        - Pagedir's destructor now frees any allocated frames, keeping mapped frames
    */
    assert(VMM::current_pagedir() != _pagedir);
    VMM::free_pagedir(_pagedir);
}

Process* Process::create() {
    Process* proc = new Process(next_pid());
    _processes.append(proc);
    return proc;
}

void Process::switch_process(Process* process) {
    assert(_processes.contains(process));
    assert(process->_kernel_esp);
    assert(process->_name);
    assert(process->_pagedir);
    assert((process->_current_ring == RING0) || (process->_current_ring == RING3));

    //TRACE("Switching to process %s (PID %u) in ring %u", process->name(), process->_pid, process->_current_ring);
    _current_process = process;
    GDT::set_kernel_stack((void*)process->_kernel_esp);

    context_t ctx;
    bzero(&ctx, sizeof(ctx));
    if(process->_current_ring == RING3) {
        ctx.cs = USER_CODE_SEG | RPL3;
        ctx.ds = USER_DATA_SEG | RPL3;
        ctx.ss = USER_DATA_SEG | RPL3;
        assert(process->_regs.eflags & EFLAGS_IF);
    } else {
        ctx.cs = KERNEL_CODE_SEG;
        ctx.ds = KERNEL_DATA_SEG;
        ctx.ss = KERNEL_DATA_SEG;
    }
    ctx.pagedir = process->_pagedir;

    ctx.regs.esp = process->_regs.esp;
    ctx.regs.eflags = process->_regs.eflags;
    ctx.regs.eip = process->_regs.eip;
    
    ctx.regs.edi = process->_regs.edi;
    ctx.regs.esi = process->_regs.esi;
    ctx.regs.edx = process->_regs.edx;
    ctx.regs.ecx = process->_regs.ecx;
    ctx.regs.ebx = process->_regs.ebx;
    ctx.regs.eax = process->_regs.eax;
    ctx.regs.ebp = process->_regs.ebp;
    switch_context(&ctx);
    PANIC("Returned from switch_context()!!!");
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
    IDT::install_handler(0x81, fork);

    // Create KERNEL_TASK
    Process* kernel_task = create();
    kernel_task->set_name("KERNEL_TASK");
    kernel_task->_kernel_esp         = KERNEL_STACK_END;
    kernel_task->_regs.esp           = KERNEL_STACK_END;
    read_eflags(kernel_task->_regs.eflags);
    kernel_task->_regs.eflags        |= EFLAGS_IF;
    kernel_task->_regs.eip           = (uint32_t)KernelTask::entry;
    kernel_task->_user_stack         = 0;
    kernel_task->_pagedir            = VMM::create_pagedir();
    TRACE("KERNEL_STACK_START: 0x%X", KERNEL_STACK_START);
    TRACE("_kernel_esp: 0x%X", kernel_task->_kernel_esp);
    kernel_task->_pagedir->alloc(KERNEL_STACK_START, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE);

    Timer::schedule(resume_next_process, nullptr, 250);
    switch_process(kernel_task);
}

void Process::exit_current_process() {
    Process* proc = _current_process;
    _current_process = next_process();
    if(_current_process == proc) {
        PANIC("No more processes to run!");
    }

    _processes.remove_val(proc);

    //VMM::switch_pagedir(_current_process->_pagedir);
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

void Process::load_elf(const char* filename) {
    /*if(_workingset_size != 0) {
        PANIC("Process image already loaded");
    }*/

    Initrd::File* file = Initrd::get().open(filename);
    if(!file) {
        PANIC("File not found: %s", filename);
    }

    const Elf32_Ehdr* hdr = elf_validate(file->data(), file->size());
    if(!hdr) {
        PANIC("Invalid ELF file: %s", filename);
    }

    //VMM::switch_pagedir(_pagedir);
    for(unsigned i = 0; i < hdr->e_phnum; i++) {
        const Elf32_Phdr* phdr = elf_segment_header(hdr, i);
        if(phdr && phdr->p_type == PT_LOAD) {
            if(phdr->p_vaddr < (uint32_t)VMM::USERSPACE_START || phdr->p_vaddr > (uint32_t)VMM::USERSPACE_END) {
                PANIC("Invalid start address: 0x%X", phdr->p_vaddr);
            }

            VMM::alloc(phdr->p_vaddr, phdr->p_memsz, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE|VMM::PAGE_USER);
            //_workingset_size = align(phdr->p_memsz, VMM::PAGE_SIZE);

            file->seek(phdr->p_offset);
            int read_bytes = file->read((uint8_t*)phdr->p_vaddr, phdr->p_filesz);
            if(read_bytes != phdr->p_filesz) {
                PANIC("Short read in file %s", filename);
            }

            for(uint8_t* bss = (uint8_t*)phdr->p_vaddr + phdr->p_filesz; bss < (uint8_t*)phdr->p_vaddr + phdr->p_memsz; bss++)
                *bss = 0;

            TRACE("Loaded segment %d at 0x%X - 0x%X", i, phdr->p_vaddr, phdr->p_memsz);
        }
    }
    delete file;

    _regs.eip = hdr->e_entry;
}

Process* Process::current_process() {
    return _current_process;
}

const char* Process::name() {
    return _name;
}

void Process::set_name(const char* name) {
    strlcpy(_name, name, sizeof(_name));
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

void Process::fork(isr_regs_t* regs) {
    // this interrupt should only be called from kernel-mode
    assert((regs->cs & 0x3) == 0);

    Process* proc = create();
    proc->set_name(_current_process->name());
    proc->_pagedir = VMM::clone_pagedir();

    assert(proc->_pagedir->is_mapped(KERNEL_STACK_START));


    proc->_kernel_stack = _current_process->_kernel_stack;
    proc->_user_stack = _current_process->_user_stack;
    proc->_current_ring = RING0;

    proc->_kernel_esp = (uint32_t)GDT::get_kernel_stack();
    proc->_regs.esp = regs->esp + 0x14;                 /* Just... dont ask, ok? */
    proc->_regs.eflags = regs->eflags;
    proc->_regs.eip = regs->eip;
    proc->_regs.edi = regs->edi;
    proc->_regs.esi = regs->esi;
    proc->_regs.edx = regs->edx;
    proc->_regs.ecx = regs->ecx;
    proc->_regs.ebx = regs->ebx;
    proc->_regs.eax = 0;                /* Child gets 0 in eax, mmmkay? */
    proc->_regs.ebp = regs->ebp;

    TRACE("proc->eip: 0x%X", proc->_regs.eip);
    regs->eax = proc->_pid;             /* And the parent receives child's PID */
}

uint8_t* Process::kernel_stack() {
    return (uint8_t*)_kernel_stack;
}

uint32_t Process::next_pid() {
    pushf();
    cli();
    uint32_t pid = _current_pid++;
    popf();
    return pid;
}



