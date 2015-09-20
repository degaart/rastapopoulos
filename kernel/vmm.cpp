#include "vmm.h"
#include "string.h"
#include "kmalloc.h"
#include "util.h"
#include "debug.h"
#include "pmm.h"

static const uint32_t KERNEL_START = 0x100000;
static const uint32_t INITIAL_KERNEL_STACK = 0x7BFF;

#define PTE_PRESENT             1
#define PTE_WRITABLE            2
#define PTE_USER                4
#define PTE_WRITETHOUGH         8
#define PTE_NOT_CACHEABLE       0x10
#define PTE_ACCESSED            0x20
#define PTE_DIRTY               0x40
#define PTE_PAT                 0x80
#define PTE_CPU_GLOBAL          0x10
#define PTE_LV4_GLOBAL          0x200
#define PTE_FRAME               0x7FFFF000
typedef uint32_t pte_t;         /* Page table entry */

#define PDE_PRESENT             1
#define PDE_WRITABLE            2
#define PDE_USER                4
#define PDE_PWT                 8
#define PDE_PCD                 0x10
#define PDE_ACCESSED            0x20
#define PDE_DIRTY               0x40
#define PDE_4MB                 0x80
#define PDE_CPU_GLOBAL          0x100
#define PDE_LV4_GLOBAL          0x200
#define PDE_FRAME               0x7FFFF000
typedef uint32_t pde_t;         /* Page directory entry */

#define PAGE_DIRECTORY_INDEX(x) (((x) >> 22) & 0x3ff)
#define PAGE_TABLE_INDEX(x) (((x) >> 12) & 0x3ff)
#define PAGE_GET_PHYSICAL_ADDRESS(x) (*x & ~0xfff)

struct pagetable_t {
    pte_t entries[1024];
};

struct pagedir_t {
    pde_t tables[1024];
};

static pagedir_t* _current_pagedir;

/* Initial map implementation, assumes paging disabled, uses kmalloc instead of PMM */
void VMM::map_seg(uint32_t va, uint32_t pa) {
    assert( va % PAGE_SIZE == 0);
    assert( pa % PAGE_SIZE == 0);

    pde_t pde = PAGE_DIRECTORY_INDEX(va);
    pte_t pte = PAGE_TABLE_INDEX(va);

    pagetable_t* page_table = (pagetable_t*)(_current_pagedir->tables[pde] & PTE_FRAME);
    if(!page_table) {
        page_table = (pagetable_t*)kmalloc_ap(sizeof(pagetable_t), PAGE_SIZE, nullptr);
        bzero(page_table, sizeof(pagetable_t));

        assert(( ((pde_t)page_table) & PDE_FRAME) == (pde_t)page_table);
        _current_pagedir->tables[pde] = (pde_t)page_table | PDE_PRESENT | PDE_WRITABLE;
    }

    assert(!page_table->entries[pte]);
    page_table->entries[pte] = pa | PTE_PRESENT | PTE_WRITABLE;

    /* Update PMM */
    PMM::reserve(pa);
}

void VMM::init() {
    _current_pagedir = (pagedir_t*)kmalloc_ap(sizeof(pagedir_t), PAGE_SIZE, nullptr);
    bzero(_current_pagedir, sizeof(sizeof(pagedir_t)));
    
    /*
        Identity-map currently allocated kernel memory
        i.e.:   INITIAL_KERNEL_STACK-0x100  -   INITIAL_KERNEL_STACK
                KERNEL_START                -   kheap_start  
    */
    for(uint32_t page = truncate(INITIAL_KERNEL_STACK - 0x100, PAGE_SIZE);
        page < align(INITIAL_KERNEL_STACK, PAGE_SIZE)+1;
        page += PAGE_SIZE
    ) {
        map_seg(page, page);
    }

    for(uint32_t page = KERNEL_START; page < align((uint32_t)kheap_start(), PAGE_SIZE) + 1; page += PAGE_SIZE) {
        map_seg(page, page);
    }
    map_seg(4096 * 1024, 4096 * 1024);
    map_seg(4096 * 1025, 4096 * 1025);

    IDT::install_handler(14, page_fault_handler);

    /* Enable paging */
    TRACE("_current_pagedir: %p", _current_pagedir);
    TRACE("&_current_pagedir: %p", &_current_pagedir);
    BREAKPOINT();
    write_cr3(_current_pagedir);
    
    uint32_t cr0;
    read_cr0(cr0);
    cr0 |= 0x80000000;
    write_cr0(cr0);
}

void VMM::page_fault_handler(const isr_regs_t* regs) {
    uint32_t faulting_addr;
    read_cr2(faulting_addr);

    int present = !(regs->err_code & 0x1);          // Page not present
    int writeop = regs->err_code & 0x2;             // Write operation?
    int usermode = regs->err_code & 0x4;            // Processor was in user-mode?
    int reserved = regs->err_code & 0x8;            // Overwritten CPU-reserved bits of page entry?
    int fetch = regs->err_code & 0x10;              // Caused by an instruction fetch?

    PANIC(
        "Page fault at address 0x%X "
        "(present: %u, writeop: %u, usermode: %u, reserved: %u, fetch: %u)",
        faulting_addr,
        present, writeop, usermode, reserved, fetch
    );
}



