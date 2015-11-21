#include "vmm.h"
#include "string.h"
#include "kmalloc.h"
#include "util.h"
#include "debug.h"
#include "pmm.h"
#include "kheap.h"
#include "pagedir.h"
#include "process.h"
#include "backtrace.h"

bool VMM::_paging_enabled = false;
Pagedir* VMM::_current_pagedir;

void VMM::init() {
    /* Create initial kernel pagedir */
    _current_pagedir = Pagedir::create();
    _current_pagedir->set_physical((uint32_t)_current_pagedir);
    
    /*
        Identity-map currently allocated kernel memory
        i.e.:   INITIAL_KERNEL_STACK-0x100  -   INITIAL_KERNEL_STACK
                KERNEL_START                -   kheap_start  
    */
    TRACE("Initial kernel stack: 0x%X - 0x%X", INITIAL_KERNEL_STACK - 0x100, INITIAL_KERNEL_STACK);
    for(uint32_t page = truncate((uint32_t)INITIAL_KERNEL_STACK - 0x100, PAGE_SIZE);
        page < align((uint32_t)INITIAL_KERNEL_STACK, PAGE_SIZE)+1;
        page += PAGE_SIZE
    ) {
        map((void*)page, page, PAGE_PRESENT | PAGE_WRITABLE);
        PMM::reserve(page);
    }

    /*
        Kernel read-only data mapped as read-only
    */
    uint32_t page = (uint32_t)_TEXT_START_;
    TRACE("_TEXT_START_: %p", _TEXT_START_);
    TRACE("_DATA_START_: %p", _DATA_START_);
    while(page < (uint32_t)_DATA_START_) {
        map((void*)page, page, PAGE_PRESENT);
        PMM::reserve(page);

        page += PAGE_SIZE;
    }

    /*
        Rest of kernel mapped read-write
        Except IDT, 'cause I hate it when you change my IDT, mmmkay?
    */
    uint32_t last_mapped_page = page;
    while(page < (uint32_t)KHeap::end()) {
        map((void*)page, page, PAGE_PRESENT | PAGE_WRITABLE);
        PMM::reserve(page);
        last_mapped_page = page;

        page += PAGE_SIZE;
    }
    TRACE("Kernel data end: 0x%X", last_mapped_page);

    IDT::install_handler(14, page_fault_handler);
    IDT::install_handler(8, double_fault_handler);

    /* Enable paging */
    TRACE("_current_pagedir: %p", _current_pagedir);
    TRACE("&_current_pagedir: %p", &_current_pagedir);
    write_cr3((uint32_t)_current_pagedir);
    
    uint32_t cr0;
    read_cr0(cr0);
    cr0 = cr0 | CR0_PG | CR0_WP; /* CR0_WP: ring0 cannot write to write-protected pages */
    write_cr0(cr0);

    _paging_enabled = true;
}

void VMM::page_fault_handler(isr_regs_t* regs) {
    uint32_t faulting_addr;
    read_cr2(faulting_addr);

    int present = regs->err_code & 0x1;
    int writeop = regs->err_code & 0x2;
    int usermode = regs->err_code & 0x4;
    int reserved = regs->err_code & 0x8;
    int fetch = regs->err_code & 0x10;

    const char* process_name = "NULL";
    if(Process::current_process())
        process_name = Process::current_process()->name();

    PANIC(
        "Page fault in process %s, at address 0x%X:0x%X, referencing address 0x%X "
        "(%s %s %s %s %s)",
        process_name,
        regs->cs, regs->eip,
        faulting_addr,
        present ? "access-violation" : "non-present-page",
        writeop ? "writeop" : "readop",
        usermode ? "user-mode" : "kernel-mode",
        reserved ? "reserved" : "",
        fetch ? "ifetch" : ""
    );
}

void VMM::double_fault_handler(isr_regs_t* regs) {
    PANIC("Double-fault exception");
}

bool VMM::paging_enabled() {
    return _paging_enabled;
}

bool VMM::get_physical(void* va, uint32_t* pa) {
    if(!paging_enabled()) {
        if(pa)
            *pa = (uint32_t)va;
        return true;
    }
    return _current_pagedir->get_physical((uint32_t)va, pa);
}

uint32_t VMM::get_page_attr(uint32_t va, uint32_t* dir_attr) {
    return _current_pagedir->get_page_attr(va, dir_attr);
}

bool VMM::allocated(void* va) {
    return _current_pagedir->allocated(va);
}

bool VMM::is_mapped(void* va) {
    return _current_pagedir->is_mapped((uint32_t)va);
}

void VMM::map(uint32_t va, uint32_t pa, uint32_t flags) {
    _current_pagedir->map(va, pa, flags);
    if(paging_enabled())
        flush_tlb((void*)va);
}

void VMM::unmap(uint32_t va) {
    _current_pagedir->unmap(va);
    if(paging_enabled())
        flush_tlb((void*)va);
}

void VMM::flush_tlb(void* va) {
    asm volatile("invlpg (%0)" ::"r" (va) : "memory");
}

Pagedir* VMM::create_pagedir() {
    return Pagedir::create();
}

void VMM::free_pagedir(Pagedir* pagedir) {
    assert(pagedir != _current_pagedir);
    delete pagedir;
}

void VMM::set_pagedir(Pagedir* pagedir) {
    pagedir->copy_kernel_mappings(_current_pagedir);
    _current_pagedir = pagedir;
}

bool VMM::alloc(uint32_t va, uint32_t size, uint32_t flags) {
    if(!size)
        return true;

    for(uint32_t page = va; page < va + align(size, VMM::PAGE_SIZE); page += VMM::PAGE_SIZE) {
        uint32_t pageframe = _current_pagedir->alloc(page, flags);
        if(!pageframe) {
            dealloc(va, page - va);
            return false;
        }
        if(paging_enabled())
            flush_tlb((void*)page);
    }
    return true;
}

void VMM::dealloc(uint32_t va, uint32_t size) {
    for(uint32_t page = va; page < va + align(size, VMM::PAGE_SIZE); page += VMM::PAGE_SIZE) {
        _current_pagedir->dealloc(page);
        if(paging_enabled())
            flush_tlb((void*)page);
    }
}

Pagedir* VMM::current_pagedir() {
    return _current_pagedir;
}

Pagedir* VMM::clone_pagedir() {
    Pagedir* pagedir = create_pagedir();

    /* Create scratch memory in kernel space */
    uint32_t* scratch = (uint32_t*)kmalloc_a(PAGE_SIZE, PAGE_SIZE);
    if(!scratch)
        return nullptr;

    /*
        Unmap scratch
        NOTE: We cant unmap it blindly, as we don't know
        if it was allocated with VMM::map() or VMM::alloc()
        with VMM::map()
    */
    if(allocated(scratch))
        VMM::dealloc(scratch);
    else {
        uint32_t scratch_pa;
        assert(VMM::get_physical(scratch, &scratch_pa));
        VMM::unmap(scratch);
        PMM::free(scratch_pa);
    }

    /* Now walk user pages in current pagedir */
    for(unsigned t = 1; t < (unsigned)(USERSPACE_END+1)/(VMM::PAGE_SIZE*1024); t++) {
        if(_current_pagedir->_entries[t] & Pagedir::PDE_PRESENT) {
            Pagedir::pagetable_t* table = _current_pagedir->_tables[t];
            assert(table);

            for(unsigned e = 0; e < 1024; e++) {
                if(table->entries[e] & Pagedir::PTE_PRESENT) {
                    uint32_t* va = (uint32_t*) ((t*VMM::PAGE_SIZE*1024) + (e*VMM::PAGE_SIZE));

                    uint32_t flags = table->entries[e] & (~Pagedir::PTE_FRAME) & (~Pagedir::PTE_ALLOCATED);
                    bool allocated = table->entries[e] & Pagedir::PTE_ALLOCATED;

                    /* Alloc new pageframe and map to scratch space */
                    uint32_t frame = PMM::alloc();

                    assert(frame != 0);
                    map(scratch, frame, flags);

                    /* Copy data into scratch space */
                    memcpy(scratch, va, VMM::PAGE_SIZE);

                    /* Map dat frame into new new pagedir and unmap from scratch */
                    pagedir->map(va, frame, allocated ? (flags | Pagedir::PTE_ALLOCATED) : flags);
                    unmap(scratch);
                }
            }
        }
    }

    /* Remap back scratch */
    alloc(scratch, VMM::PAGE_PRESENT|VMM::PAGE_WRITABLE);

    /* And free it */
    kfree(scratch);
    return pagedir;
}


