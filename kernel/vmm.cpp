#include "vmm.h"
#include "string.h"
#include "kmalloc.h"
#include "util.h"
#include "debug.h"
#include "pmm.h"
#include "kheap.h"

bool VMM::_paging_enabled = false;
static const uint32_t INITIAL_KERNEL_STACK = 0x7BFF;
VMM::pagedir_t* VMM::_current_pagedir;

#define PAGE_DIRECTORY_INDEX(x) (((x) >> 22) & 0x3ff)
#define PAGE_TABLE_INDEX(x) (((x) >> 12) & 0x3ff)
#define PAGE_GET_PHYSICAL_ADDRESS(x) (*x & ~0xfff)

void VMM::init() {
    /* Create initial kernel pagedir */
    uint32_t pagedir_physical;
    _current_pagedir = (pagedir_t*)kmalloc_ap(sizeof(pagedir_t), PAGE_SIZE, &pagedir_physical);
    _current_pagedir->physical = pagedir_physical;
    assert((uint32_t)_current_pagedir->tables == pagedir_physical);

    bzero(_current_pagedir, sizeof(sizeof(pagedir_t)));
    
    /*
        Identity-map currently allocated kernel memory
        i.e.:   INITIAL_KERNEL_STACK-0x100  -   INITIAL_KERNEL_STACK
                KERNEL_START                -   kheap_start  
    */
    TRACE("Initial kernel stack: 0x%X - 0x%X", INITIAL_KERNEL_STACK - 0x100, INITIAL_KERNEL_STACK);
    for(uint32_t page = truncate(INITIAL_KERNEL_STACK - 0x100, PAGE_SIZE);
        page < align(INITIAL_KERNEL_STACK, PAGE_SIZE)+1;
        page += PAGE_SIZE
    ) {
        map_seg(page, page, PTE_WRITABLE);
        PMM::reserve(page);
    }

    /*
        Kernel read-only data mapped as read-only
    */
    uint32_t page = (uint32_t)_TEXT_START_;
    TRACE("_TEXT_START_: %p", _TEXT_START_);
    TRACE("_DATA_START_: %p", _DATA_START_);
    while(page < (uint32_t)_DATA_START_) {
        map_seg(page, page, 0);
        PMM::reserve(page);

        page += PAGE_SIZE;
    }

    /*
        Rest of kernel mapped read-write
    */
    uint32_t last_mapped_page = page;
    while(page < (uint32_t)KHeap::end()) {
        map_seg(page, page, PTE_WRITABLE);
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
    write_cr3(pagedir_physical);
    
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
        "Page fault at address 0x%X "
        "(%s %s %s %s %s)",
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

bool VMM::get_physical(uint32_t va, uint32_t* pa) {
    assert(pa);

    unsigned dir_index = PAGE_DIRECTORY_INDEX(va);
    if(_current_pagedir->tables[dir_index] & PDE_PRESENT) {
        pagetable_t* pagetable = (pagetable_t*) (_current_pagedir->tables[dir_index] & PDE_FRAME);
        unsigned table_index = PAGE_TABLE_INDEX(va);
        if(pagetable->entries[table_index] & PTE_PRESENT) {
            uint32_t frame = pagetable->entries[table_index] & PTE_FRAME;
            uint32_t offset = va & PTE_OFFSET;
            *pa = frame + offset;
            return true;
        }
    }
    return false;
}

bool VMM::is_mapped(uint32_t va) {
    unsigned dir_index = PAGE_DIRECTORY_INDEX(va);
    if(!(_current_pagedir->tables[dir_index] & PDE_PRESENT))
        return false;

    unsigned table_index = PAGE_TABLE_INDEX(va);
    pagetable_t* page_table = (pagetable_t*)(_current_pagedir->tables[dir_index] & PDE_FRAME);
    if(!(page_table->entries[table_index] & PTE_PRESENT))
        return false;
    return true;
}
 
/* Initial map implementation, assumes paging disabled, uses kmalloc instead of PMM */
void VMM::map_seg(uint32_t va, uint32_t pa, uint32_t flags) {
    assert(!_paging_enabled);
    assert( va % PAGE_SIZE == 0);
    assert( pa % PAGE_SIZE == 0);

    pde_t pde = PAGE_DIRECTORY_INDEX(va);
    pte_t pte = PAGE_TABLE_INDEX(va);

    pagetable_t* page_table = (pagetable_t*)(_current_pagedir->tables[pde] & PTE_FRAME);
    if(!page_table) {
        uint32_t table_physical;
        page_table = (pagetable_t*)kmalloc_ap(sizeof(pagetable_t), PAGE_SIZE, &table_physical);
        bzero(page_table, sizeof(pagetable_t));

        assert(( ((pde_t)page_table) & PDE_FRAME) == (pde_t)page_table);
        _current_pagedir->tables[pde] = (pde_t)page_table | PDE_PRESENT | PDE_WRITABLE;
        _current_pagedir->tables_physical[pde] = table_physical;
    }

    assert(!page_table->entries[pte]);
    page_table->entries[pte] = pa | PTE_PRESENT | flags;
}

void VMM::map(uint32_t va, uint32_t pa, uint32_t flags, uint32_t options) {
    assert(paging_enabled());
    if(va % PAGE_SIZE)
        PANIC("Invalid VA: 0x%X", va);
    if(pa % PAGE_SIZE)
        PANIC("Invalid PA: 0x%X", pa);

    pde_t pde = PAGE_DIRECTORY_INDEX(va);
    pte_t pte = PAGE_TABLE_INDEX(va);

    pagetable_t* page_table = (pagetable_t*)(_current_pagedir->tables[pde] & PTE_FRAME);
    if(!page_table || !(page_table->entries[pte] & PTE_PRESENT)) {
        uint32_t table_physical;
        page_table = (pagetable_t*)kmalloc_ap(sizeof(pagetable_t), PAGE_SIZE, &table_physical);
        bzero(page_table, sizeof(pagetable_t));

        assert(( ((pde_t)page_table) & PDE_FRAME) == (pde_t)page_table);
        _current_pagedir->tables[pde] = (pde_t)page_table | PDE_PRESENT | PDE_WRITABLE;
        _current_pagedir->tables_physical[pde] = table_physical;
    }

    if(page_table->entries[pte] && !(options & MAP_REMAP)) {
        PANIC(
            "VA 0x%X already mapped to PA 0x%X, flags 0x%X (trying to remap to PA 0x%X, flags 0x%X)",
            va, pa,
            page_table->entries[pte] & (~PTE_FRAME),
            pa, flags
        );
    }

    page_table->entries[pte] = pa | flags;
}


