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
#include "syscall.h"
#include "io.h"
#include "kmalloc.h"

#define EPHEMERAL_PORT_START 65536

Process::ProcessList_t Process::_processes;
Process* Process::_current_process = nullptr;
uint32_t Process::_current_pid = 0;
uint32_t Process::_current_ephemeral_port = EPHEMERAL_PORT_START;
Process::ProcessList_t Process::_ready_queue;
Process::ProcessList_t Process::_msgwait_queue;
Process::ProcessList_t Process::_sleep_queue;
Process::ProcessList_t Process::_exited_queue;

Process::Process(uint32_t pid)
: _pid(pid) {
    bzero(_name, sizeof(_name));
    _pagedir        = nullptr;
    _kernel_stack   = 0;
    _user_stack     = 0;
    _current_ring   = RING0;
    _kernel_esp     = 0;
    bzero(&_regs, sizeof(_regs));
}

Process::~Process() {
    /* Walk each queue, the current process shouldn't be in any of them */
    assert(this != _current_process);
    for(auto i = _ready_queue.iterator(); i.valid(); i.next())
        assert(this != *i);
    for(auto i = _msgwait_queue.iterator(); i.valid(); i.next())
        assert(this != *i);
    for(auto i = _sleep_queue.iterator(); i.valid(); i.next())
        assert(this != *i);
    for(auto i = _exited_queue.iterator(); i.valid(); i.next())
        assert(this != *i);
    for(auto i = _processes.iterator(); i.valid(); i.next())
        assert(this != *i);

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

void Process::switch_process() {
    /* dump some infos for debugging */
    // dump_queues_compact();

    Process* process = _ready_queue.pop();

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

void Process::timer_schedule(void* args, const isr_regs_t* regs) {
    save_context(regs);

    /* Process sleep queue (they should be woken up as soon as possible) */
    uint64_t now = Timer::current_timestamp();
    ProcessList_t woken;
    for(auto process = _sleep_queue.iterator(); process.valid(); process.next()) {
        if(now >= (*process)->_sleep_deadline) {
            woken.append(*process);
        }
    }
    for(auto process = woken.iterator(); process.valid(); process.next()) {
        _sleep_queue.remove_val(*process);
        _ready_queue.push(*process);            /* Put at top of ready queue */
    }

    _ready_queue.append(_current_process);
    switch_process();
    PANIC("Should not happen");
}

void Process::timer_print_current_process(void* args, const isr_regs_t* regs) {
    if(_current_process) {
        char buffer[10];
        String::itoa(buffer, _current_process->pid());
        Debug::write_string(buffer);
    }
}

void Process::init() {
    assert(!interrupts_enabled());

    Syscall::install_handler(SYSCALL_PORT_OPEN, syscall_port_open);
    Syscall::install_handler(SYSCALL_PORT_CLOSE, syscall_port_close);
    Syscall::install_handler(SYSCALL_PORT_SEND, syscall_port_send);
    Syscall::install_handler(SYSCALL_PORT_READ, syscall_port_read);
    Syscall::install_handler(SYSCALL_FORK, syscall_fork);
    Syscall::install_handler(SYSCALL_YIELD, syscall_yield);

    // Create KERNEL_TASK
    Process* kernel_task            = create();
    kernel_task->set_name("KERNEL_TASK");
    kernel_task->_kernel_esp        = KERNEL_STACK_END;
    kernel_task->_regs.esp          = KERNEL_STACK_END;
    read_eflags(kernel_task->_regs.eflags);
    kernel_task->_regs.eflags       |= EFLAGS_IF;
    kernel_task->_regs.eip          = (uint32_t)KernelTask::entry;
    kernel_task->_user_stack        = 0;
    kernel_task->_pagedir           = VMM::create_pagedir();
    kernel_task->_pagedir->alloc(KERNEL_STACK_START, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE);
    kernel_task->_kernel_stack      = KERNEL_STACK_START;
    _ready_queue.append(kernel_task);

    Timer::schedule(timer_schedule, nullptr, 250);
    Timer::schedule(timer_print_current_process, nullptr, 40);
    switch_process();
}

Process* Process::next_process() {
    assert(_processes.size() != 0);
    return _ready_queue.head();
}

void Process::load_elf(const char* filename) {
    /* TODO: handle the case when already loaded */
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
            PANIC("Process %s: Access violation (unreadable page) at page 0x%X. Terminated", _name, first_unreadable);
        } else {
            PANIC("Process %s: Unmapped page at 0x%X. Terminated", _name, first_invalid);
        }
    }
}

void Process::check_writable_block(const void* buffer, size_t size) {
    assert(this == _current_process);

    uint32_t first_readonly, first_invalid;
    if(!_pagedir->check_writable_block(buffer, size, &first_readonly, &first_invalid)) {
        if(first_readonly) {
            PANIC("Process %s: Access violation (read-only) at page at 0x%X. Terminated", _name, first_readonly);
        } else {
            PANIC("Process %s: Unmapped page at 0x%X. Terminated", _name, first_invalid);
        }
    }
}

uint8_t* Process::kernel_stack() {
    return (uint8_t*)_kernel_stack;
}

uint32_t Process::next_pid() {
    uint32_t lock;
    EnterCriticalSection(lock);
    uint32_t pid = _current_pid++;
    LeaveCriticalSection(lock);
    return pid;
}

void Process::save_context(const isr_regs_t* regs) {
    assert(_current_process != nullptr);

    _current_process->_current_ring = regs->cs & 0x3 ? RING3 : RING0;

    if(_current_process->_current_ring == RING3)
        _current_process->_regs.esp     = regs->useresp;
    else
        _current_process->_regs.esp     = regs->esp + 0x14;

    _current_process->_kernel_esp       = (uint32_t)GDT::get_kernel_stack();
    _current_process->_regs.eflags      = regs->eflags;
    _current_process->_regs.eip         = regs->eip;
    _current_process->_regs.edi         = regs->edi;
    _current_process->_regs.esi         = regs->esi;
    _current_process->_regs.edx         = regs->edx;
    _current_process->_regs.ecx         = regs->ecx;
    _current_process->_regs.ebx         = regs->ebx;
    _current_process->_regs.eax         = regs->eax;
    _current_process->_regs.ebp         = regs->ebp;
}

uint32_t Process::open_port(uint32_t port_number) {
    if(port_number == INVALID_PORT)
        port_number = _current_ephemeral_port++;
    else if(port_number >= EPHEMERAL_PORT_START)
        return INVALID_PORT;

    /* Check if another process hasn't opened that port */
    Process* p = process_for_port(port_number);
    if(p != nullptr) {
        TRACE("Process %u has already opened port %d", p->_pid, port_number);
        return INVALID_PORT;
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

uint32_t Process::syscall_port_open(uint32_t port_number, uint32_t unused0, uint32_t unused1, isr_regs_t* regs) {
    return _current_process->open_port(port_number);
}

uint32_t Process::syscall_port_close(uint32_t port_number, uint32_t unused0, uint32_t unused1, isr_regs_t* regs) {
    return _current_process->close_port(port_number);
}

uint32_t Process::syscall_port_send(uint32_t port_number, uint32_t msg_addr, uint32_t unused1, isr_regs_t* regs) {
    const Message_t* msg = (const Message_t*)msg_addr;

    /* Check message validity */
    if(!msg)
        return Port::INVALID_MESSAGE;
    // _current_process->check_readable_block(msg, sizeof(Message_t));
    // _current_process->check_readable_block(msg->payload, msg->payload_size);
    
    Process* dst_proc = Process::process_for_port(port_number);
    if(dst_proc == nullptr)
        return Port::INVALID_PORT_NUMBER;

    Port* port = dst_proc->get_port(port_number);
    if(port == nullptr)
        return Port::INVALID_PORT_NUMBER;

    uint32_t ret = port->send(*msg, _current_process);
    if(ret != 0)
        return ret;

    /* Update process state */
    if(_msgwait_queue.contains(dst_proc)) {
        _msgwait_queue.remove_val(dst_proc);
        _ready_queue.append(dst_proc);
    }
    return Port::SUCCESS;
}

uint32_t Process::syscall_port_read(uint32_t port_number, uint32_t msg_addr, uint32_t unused1, isr_regs_t* regs) {
    Message_t* buffer = (Message_t*)msg_addr;
    if(!buffer)
        return Port::INVALID_MESSAGE;
    // proc->check_writable_block(buffer, sizeof(Message_t));
    // proc->check_writable_block(buffer->payload, buffer->payload_size);

    Port* port = _current_process->get_port(port_number);
    if(!port)
        return Port::INVALID_PORT_NUMBER;
    
    while(port->empty()) {
        Syscall::syscall(SYSCALL_YIELD, YIELD_MSGWAIT);
    }

    int32_t ret = port->read(buffer);
    return ret;
}

uint32_t Process::syscall_fork(uint32_t unused0, uint32_t unused1, uint32_t unused2, isr_regs_t* regs) {
    Process* proc           = create();
    proc->set_name(_current_process->name());
    proc->_pagedir          = VMM::clone_pagedir();
    assert(proc->_pagedir->is_mapped(KERNEL_STACK_START));

    proc->_kernel_stack     = _current_process->_kernel_stack;
    proc->_user_stack       = _current_process->_user_stack;
    proc->_current_ring     = _current_process->_current_ring;

    proc->_kernel_esp       = (uint32_t)GDT::get_kernel_stack();
    proc->_regs.esp         = regs->esp + 0x14;                 /* Just... dont ask, ok? */
    proc->_regs.eflags      = regs->eflags;
    proc->_regs.eip         = regs->eip;
    proc->_regs.edi         = regs->edi;
    proc->_regs.esi         = regs->esi;
    proc->_regs.edx         = regs->edx;
    proc->_regs.ecx         = regs->ecx;
    proc->_regs.ebx         = regs->ebx;
    proc->_regs.eax         = 0;                                /* Child gets 0 in eax, mmmkay? */
    proc->_regs.ebp         = regs->ebp;
    _ready_queue.append(proc);

    return proc->_pid;                                          /* And the parent receives child's PID */
}

uint32_t Process::syscall_yield(uint32_t flags, uint32_t duration, uint32_t unused1, isr_regs_t* regs) {
    if(flags == YIELD_MSGWAIT) {
        if(_processes.size() == 1) {
            PANIC("Deadlock: there is no other process to give you messages, dumbass!");
        } else {
            _msgwait_queue.push(_current_process);    
        }
    } else if(flags == YIELD_SLEEP) {
        assert(_processes.contains(_current_process));
        assert(!_sleep_queue.contains(_current_process));
        //Process not in _ready_queue because it was removed by switch_process()

        _ready_queue.remove_val(_current_process);

        _current_process->_sleep_deadline = Timer::current_timestamp() + duration;
        _sleep_queue.push(_current_process);
    } else {
        if(_ready_queue.size() == 1) {
            sti();
            yield();
            cli();
            return 0;
        }
        _ready_queue.append(_current_process);
    }

    save_context(regs);
    switch_process();
    return 0;
}

void Process::dump_queues() {
    TRACE("Ready queue:");
    for(auto i = _ready_queue.iterator(); i.valid(); i.next()) {
        TRACE("\t%d\t%s", (*i)->_pid, (*i)->_name);
    }

    TRACE("MsgWait queue:");
    for(auto i = _msgwait_queue.iterator(); i.valid(); i.next()) {
        TRACE("\t%d\t%s", (*i)->_pid, (*i)->_name);
    }

    TRACE("Sleep queue:");
    for(auto i = _sleep_queue.iterator(); i.valid(); i.next()) {
        TRACE("\t%d\t%s", (*i)->_pid, (*i)->_name);
    }

    TRACE("Exited queue:");
    for(auto i = _exited_queue.iterator(); i.valid(); i.next()) {
        TRACE("\t%d\t%s", (*i)->_pid, (*i)->_name);
    }

    TRACE("All processes:");
    for(auto i = _processes.iterator(); i.valid(); i.next()) {
        TRACE("\t%d\t%s", (*i)->_pid, (*i)->_name);
    }
}

void Process::dump_queues_compact() {
    char* str = (char*)kmalloc(4096);
    *str = '\0';
    strlcat(str, "\n", 4096);
    
    if(_ready_queue.size()) {
        sncatf(str, 4096, "Ready queue:");
        for(auto i = _ready_queue.iterator(); i.valid(); i.next()) {
            sncatf(str, 4096, " %d", (*i)->_pid);
        }
        strlcat(str, "\n", 4096);
    }
    if(_msgwait_queue.size()) {
        sncatf(str, 4096, "Msgwait queue:");
        for(auto i = _msgwait_queue.iterator(); i.valid(); i.next()) {
            sncatf(str, 4096, " %d", (*i)->_pid);
        }
         strlcat(str, "\n", 4096);
    }
    if(_sleep_queue.size()) {
        sncatf(str, 4096, "Sleep queue:");
        for(auto i = _sleep_queue.iterator(); i.valid(); i.next()) {
            sncatf(str, 4096, " %d", (*i)->_pid);
        }
         strlcat(str, "\n", 4096);
    }
    if(_exited_queue.size()) {
        sncatf(str, 4096, "Exited queue:");
        for(auto i = _exited_queue.iterator(); i.valid(); i.next()) {
            sncatf(str, 4096, " %d", (*i)->_pid);
        }
         strlcat(str, "\n", 4096);
    }
    if(strlen(str) && str[strlen(str)-1] == '\n')
        str[strlen(str)-1] = '\0';

    TRACE("%s", str);
    kfree(str);
}

