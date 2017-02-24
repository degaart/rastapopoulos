#include "vmm.h"
#include "debug.h"
#include "pmm.h"
#include "kmalloc.h"
#include "registers.h"
#include "string.h"
#include "util.h"
#include "kernel.h"

#define PTE_PRESENT             (1)
#define PTE_WRITABLE            (1 << 1)
#define PTE_USER                (1 << 2)
#define PTE_WRITETHOUGH         (1 << 3)
#define PTE_NOT_CACHEABLE       (1 << 4)
#define PTE_ACCESSED            (1 << 5)
#define PTE_DIRTY               (1 << 6)
#define PTE_PAT                 (1 << 7)
#define PTE_CPU_GLOBAL          (1 << 8)
#define PTE_AVL0                (1 << 9)
#define PTE_AVL1                (1 << 10)
#define PTE_AVL2                (1 << 11)
#define PTE_FRAME               0xFFFFF000
#define PTE_OFFSET              0x00000FFF

#define PDE_PRESENT             (1)
#define PDE_WRITABLE            (1 << 1)
#define PDE_USER                (1 << 2)
#define PDE_PWT                 (1 << 3)
#define PDE_PCD                 (1 << 4)
#define PDE_ACCESSED            (1 << 5)
#define PDE_AVL0                (1 << 6) /* Used only if 4mb page */
#define PDE_AVL1                (1 << 7) /* Used only if 4mb page */
#define PDE_AVL2                (1 << 8)
#define PDE_AVL3                (1 << 9)
#define PDE_AVL4                (1 << 10)
#define PDE_AVL5                (1 << 11)
#define PDE_FRAME               0xFFFFF000

#define PAGE_DIRECTORY_INDEX(x) (((x) >> 22) & 0x3ff)
#define PAGE_TABLE_INDEX(x) (((x) >> 12) & 0x3ff)
#define PAGE_GET_PHYSICAL_ADDRESS(x) (*x & ~0xfff)

struct pagedir {
    uint32_t entries[1024];
};

struct pagetable {
    uint32_t entries[1024];
};

static struct pagedir* current_pagedir = NULL;
extern unsigned char initial_kernel_stack[];
static bool paging_enabled = false;

static void vmm_map_linear(struct pagedir* pagedir, uint32_t va, uint32_t pa, uint32_t flags);

void vmm_init()
{
    assert(pmm_initialized());

    /* Create initial kernel pagedir */
    struct pagedir* pagedir = kmalloc_a(sizeof(struct pagedir), PAGE_SIZE);
    bzero(pagedir, sizeof(struct pagedir));

    /* Identity-map first 16Mb */
    for(uint32_t page = 0; page < 16 * 1024 * 1024; page += PAGE_SIZE) {
        vmm_map_linear(pagedir, page, page, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
    }

    /* 
     * Last entry in pagedir should point to itself so we can modify it when paging is enabled
     * (see recursive mapping)
     */
    assert((uint32_t)pagedir == (((uint32_t)pagedir) & PDE_FRAME));
    pagedir->entries[1023] = ((uint32_t)pagedir) | PDE_PRESENT | PDE_WRITABLE;

#if 0
    /* Map initial kernel stack */
    vmm_map(initial_kernel_stack, 
            initial_kernel_stack, 
            VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);

    /* Kernel .text */
    for(uint32_t page = (uint32_t)_TEXT_START_; page < (uint32_t)_TEXT_END_; page += PAGE_SIZE) {
        vmm_map(page, page, VMM_PAGE_PRESENT);
    }

    /* Kernel .rodata */
    for(uint32_t page = (uint32_t)_RODATA_START_; page < (uint32_t)_RODATA_END_; page += PAGE_SIZE) {
        vmm_map(page, page, VMM_PAGE_PRESENT);
    }

    /* Kernel .data */
    for(uint32_t page = (uint32_t)_DATA_START_; page < (uint32_t)_DATA_END_; page += PAGE_SIZE) {
        vmm_map(page, page, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
    }

    /* Kernel .bss */
    for(uint32_t page = (uint32_t)_BSS_START_; page < (uint32_t)_BSS_END_; page += PAGE_SIZE) {
        vmm_map(page, page, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
    }

    /* Kernel heap */
    struct kernel_heap_info heap_info;
    kernel_heap_info(&heap_info);
    for(uint32_t page = heap_info.heap_start; page < heap_info.heap_start + heap_info.heap_size; page += PAGE_SIZE) {
        vmm_map(page, page, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
    }
#endif

    /* Enable paging */
    write_cr3((uint32_t)pagedir);
    uint32_t cr3 = read_cr3();
    assert(cr3 == (uint32_t)pagedir);

    uint32_t cr0 = read_cr0();
    cr0 |= CR0_PG | CR0_WP; /* CR0_WP: ring0 cannot write to write-protected pages */
    write_cr0(cr0);

    cr0 = read_cr0();

    assert(cr0 & CR0_PG);
    assert(cr0 & CR0_WP);

    paging_enabled = true;
}

/*
 * Map pages while paging is still disabled
 */
static void vmm_map_linear(struct pagedir* pagedir, uint32_t va, uint32_t pa, uint32_t flags)
{
    assert(!paging_enabled);

    assert(IS_ALIGNED(va, PAGE_SIZE));
    assert(IS_ALIGNED(pa, PAGE_SIZE));

    uint32_t dir_index = PAGE_DIRECTORY_INDEX(va);
    uint32_t table_index = PAGE_TABLE_INDEX(va);

    /* Check if present in page directory */
    if(!(pagedir->entries[dir_index] & PDE_PRESENT)) {
        struct pagetable* table = kmalloc_a(sizeof(struct pagetable), PAGE_SIZE);
        assert(IS_ALIGNED((uint32_t)table, PAGE_SIZE));
        bzero(table, sizeof(struct pagetable));

        pagedir->entries[dir_index] = ((uint32_t)table) | PDE_PRESENT | PDE_USER | PDE_WRITABLE;
    }

    /* Now check if present in pagetable */
    struct pagetable* table = (struct pagetable*)(pagedir->entries[dir_index] & PDE_FRAME);
    if(table->entries[table_index] & PTE_PRESENT) {
        trace("VA %p already mapped", va);
        abort();
    }

    table->entries[table_index] = (va & PTE_FRAME) | flags;
}

/*
 * Map pages while paging is enabled
 */
void vmm_map(uint32_t va, uint32_t pa, uint32_t flags)
{
    assert(paging_enabled);

    assert(IS_ALIGNED(va, PAGE_SIZE));
    assert(IS_ALIGNED(pa, PAGE_SIZE));

    uint32_t dir_index = PAGE_DIRECTORY_INDEX(va);
    uint32_t table_index = PAGE_TABLE_INDEX(va);

    /* Current pagedir available at 0xFFFFF000 because of recursive mapping */
    struct pagedir* current_pagedir = (struct pagedir*)0xFFFFF000;

    /* Check if present in page directory */
    bool pde_present = current_pagedir->entries[dir_index] & PDE_PRESENT;
    if(!pde_present) {
        /* Alloc 4k frame for page table */
        uint32_t table_pa = pmm_alloc();

        /* And stash into pagedir */
        current_pagedir->entries[dir_index] = ((uint32_t)table_pa) | PDE_PRESENT | PDE_USER | PDE_WRITABLE;

        /*
         * Reload cr3
         * TODO: Replace with flush_tlb()
         */
        write_cr3(read_cr3());

        struct pagetable* table = (struct pagetable*)(0xFFC00000 + (dir_index * PAGE_SIZE));
        bzero(table, sizeof(struct pagetable));
        table->entries[table_index] = (pa & PTE_FRAME) | flags;
    } else {
        /* Pagetables available at (0xFFC00000 + (pte * PAGE_SIZE)) */
        struct pagetable* table = (struct pagetable*)(0xFFC00000 + (dir_index * PAGE_SIZE));

        if(table->entries[table_index] & PTE_PRESENT) {
            trace("VA %p already mapped", va);
            abort();
        }

        table->entries[table_index] = (pa & PTE_FRAME) | flags;
    }

    /* TODO: Replace with flush_tlb */
    write_cr3(read_cr3());
    //vmm_flush_tlb(va);
}

void vmm_remap(uint32_t va, uint32_t flags)
{
    assert(paging_enabled);

    assert(IS_ALIGNED(va, PAGE_SIZE));
    assert(flags & VMM_PAGE_PRESENT);       /* Use vmm_unmap to unmap */

    uint32_t dir_index = PAGE_DIRECTORY_INDEX(va);
    uint32_t table_index = PAGE_TABLE_INDEX(va);

    /* Current pagedir available at 0xFFFFF000 because of recursive mapping */
    struct pagedir* current_pagedir = (struct pagedir*)0xFFFFF000;

    /* Check if present in page directory */
    bool pde_present = current_pagedir->entries[dir_index] & PDE_PRESENT;
    assert(pde_present);

    /* Pagetables available at (0xFFC00000 + (pte * PAGE_SIZE)) */
    struct pagetable* table = (struct pagetable*)(0xFFC00000 + (dir_index * PAGE_SIZE));
    assert(table->entries[table_index] & PTE_PRESENT);

    /* This will surely fail when we start using other bits of page table entries (e.g. PTE_ACCESSED) */
    uint32_t frame = table->entries[table_index] & PTE_FRAME;
    table->entries[table_index] = frame | flags;

    write_cr3(read_cr3());
}

void vmm_flush_tlb(uint32_t va)
{
    invlpg(va);
}

void vmm_unmap(uint32_t va)
{
    assert(paging_enabled);
    assert(IS_ALIGNED(va, PAGE_SIZE));

    uint32_t dir_index = PAGE_DIRECTORY_INDEX(va);
    uint32_t table_index = PAGE_TABLE_INDEX(va);

    struct pagedir* current_pagedir = (struct pagedir*)0xFFFFF000;
    bool pde_present = current_pagedir->entries[dir_index] & PDE_PRESENT;
    assert(pde_present);

    struct pagetable* table = (struct pagetable*)(0xFFC00000 + (dir_index * PAGE_SIZE));
    assert(table->entries[table_index] & PTE_PRESENT);

    table->entries[table_index] &= ~PTE_PRESENT;

    write_cr3(read_cr3());
}





