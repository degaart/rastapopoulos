#include "vmm.h"
#include "string.h"
#include "kmalloc.h"
#include "util.h"
#include "debug.h"
#include "pmm.h"
#include "kheap.h"

extern "C" void _flush_tlb(uint32_t);

bool VMM::_paging_enabled = false;
// static const uint32_t INITIAL_KERNEL_STACK = 0x7BFF;
const uint8_t* VMM::INITIAL_KERNEL_STACK = reinterpret_cast<uint8_t*>(0x7BFF);
VMM::pagedir_t* VMM::_current_pagedir;

#define PAGE_DIRECTORY_INDEX(x) (((x) >> 22) & 0x3ff)
#define PAGE_TABLE_INDEX(x) (((x) >> 12) & 0x3ff)
#define PAGE_GET_PHYSICAL_ADDRESS(x) (*x & ~0xfff)

void VMM::init() {
    /* Create initial kernel pagedir */
    _current_pagedir = (pagedir_t*)kmalloc_a(sizeof(pagedir_t), PAGE_SIZE);
    bzero(_current_pagedir, sizeof(pagedir_t));    
    _current_pagedir->physical = (uint32_t)_current_pagedir;
    
    /*
        Identity-map currently allocated kernel memory
        i.e.:   INITIAL_KERNEL_STACK-0x100  -   INITIAL_KERNEL_STACK
                KERNEL_START                -   kheap_start  
    */
    TRACE("Initial kernel stack: %p - %p", INITIAL_KERNEL_STACK - 0x100, INITIAL_KERNEL_STACK);
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
    cr0 = cr0 | CR0_PG | CR0_WP;
    write_cr0(cr0);

    _paging_enabled = true;
}

void VMM::page_fault_handler(const isr_regs_t* regs) {
    uint32_t faulting_addr;
    read_cr2(faulting_addr);

    int present = regs->err_code & 0x1;
    int writeop = regs->err_code & 0x2;
    int usermode = regs->err_code & 0x4;
    int reserved = regs->err_code & 0x8;
    int fetch = regs->err_code & 0x10;

    PANIC(
        "Page fault at address 0x%X:0x%X, referencing address 0x%X "
        "(%s %s %s %s %s)",
        regs->cs, regs->eip,
        faulting_addr,
        present ? "access-violation" : "non-present-page",
        writeop ? "writeop" : "readop",
        usermode ? "user-mode" : "kernel-mode",
        reserved ? "reserved" : "",
        fetch ? "ifetch" : ""
    );
}

void VMM::double_fault_handler(const isr_regs_t* regs) {
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

    assert(pa);

    uint32_t iva = (uint32_t)va;
    unsigned dir_index = PAGE_DIRECTORY_INDEX(iva);
    if(_current_pagedir->entries[dir_index] & PDE_PRESENT) {
        pagetable_t* pagetable = _current_pagedir->tables[dir_index];
        unsigned table_index = PAGE_TABLE_INDEX(iva);
        if(pagetable->entries[table_index] & PTE_PRESENT) {
            uint32_t frame = pagetable->entries[table_index] & PTE_FRAME;
            uint32_t offset = iva & PTE_OFFSET;
            if(pa)
                *pa = frame + offset;
            return true;
        }
    }
    return false;
}

bool VMM::is_mapped(void* va) {
    return get_physical(va, nullptr);
}

void VMM::map(void* va, uint32_t pa, uint32_t flags, uint32_t options) {
    uint32_t iva = (uint32_t)va;
    if(iva % PAGE_SIZE)
        PANIC("Invalid VA: 0x%X", iva);
    if(pa % PAGE_SIZE)
        PANIC("Invalid PA: 0x%X", pa);


    uint32_t dir_index = PAGE_DIRECTORY_INDEX(iva);
    uint32_t table_index = PAGE_TABLE_INDEX(iva);

    pagetable_t* page_table = _current_pagedir->tables[dir_index];
    if(!page_table || !(_current_pagedir->entries[dir_index] & PTE_PRESENT)) {
        /* Kmalloc can't get physical address anymore now. We must do the grunt work of decoding pagetable to get physical address */
        uint32_t table_physical;
        page_table = (pagetable_t*)kmalloc_a(sizeof(pagetable_t), PAGE_SIZE);
        assert(get_physical(page_table, &table_physical));
        TRACE("Allocated new pagetable: %p (physical 0x%X)", page_table, table_physical);
        bzero(page_table, sizeof(pagetable_t));

        _current_pagedir->entries[dir_index] = (table_physical & PDE_FRAME) | PDE_PRESENT | PDE_WRITABLE | PDE_USER; /* TODO: Remove PDE_USER for kernel code & heap */
        _current_pagedir->tables[dir_index] = page_table;
    }

    if((page_table->entries[table_index] & PDE_PRESENT) && !(options & MAP_REMAP)) {
        PANIC(
            "VA 0x%X already mapped to PA 0x%X, flags 0x%X (trying to remap to PA 0x%X, flags 0x%X)",
            iva, pa,
            page_table->entries[table_index] & (~PTE_FRAME),
            pa, flags
        );
    }
    page_table->entries[table_index] = pa | flags | PTE_USER; /* TODO: Remove PTE_USER for kernel code & heap */

    if(paging_enabled())
        flush_tlb(va);

    // TRACE("Mapped 0x%X to 0x%X", iva, pa);
}

void VMM::flush_tlb(void* va) {
    _flush_tlb((uint32_t)va);
}



