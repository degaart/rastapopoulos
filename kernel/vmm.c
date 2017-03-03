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
#define PTE_FLAGS               0x00000FFF

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
#define PDE_FLAGS               0x00000FFF

#define PAGE_DIRECTORY_INDEX(x) (((x) >> 22) & 0x3ff)
#define PAGE_TABLE_INDEX(x) (((x) >> 12) & 0x3ff)
#define PAGE_GET_PHYSICAL_ADDRESS(x) (*x & ~0xfff)

#define KERNEL_LO_PDE                   0
#define USER_PDE_START                  1
#define USER_PDE_END                    (PAGE_DIRECTORY_INDEX(KERNEL_HI_START) - 1)
#define KERNEL_HI_PDE_START             PAGE_DIRECTORY_INDEX(KERNEL_HI_START)
#define KERNEL_HI_PDE_END               1022
#define RECURSIVE_MAPPING_PDE           1023

struct pagedir {
    uint32_t entries[1024];
};

struct pagetable {
    uint32_t entries[1024];
};

struct va_info {
    unsigned dir_index;
    unsigned table_index;
};

struct va_info_ex {
    struct va_info info;

    uint32_t pde;
    uint32_t pte;

    uint32_t frame;
    uint32_t flags;
};

static bool             paging_enabled = false;

static void vmm_map_linear(struct pagedir* pagedir, uint32_t va, uint32_t pa, uint32_t flags);
extern void invlpg(uint32_t va);

static struct va_info va_info(void* va)
{
    struct va_info result;
    result.dir_index = PAGE_DIRECTORY_INDEX((uint32_t)va);
    result.table_index = PAGE_TABLE_INDEX((uint32_t)va);
    return result;
}

static void flush_tlb()
{
    write_cr3(read_cr3());
}

static struct pagedir* current_pagedir()
{
    assert(paging_enabled);

#if 0
    /* Current pagedir available at 0xFFFFF000 because of recursive mapping */
    struct pagedir* result = (struct pagedir*)0xFFFFF000;
    return result;
#endif

    /* 
     * but current pagedir is also guaranteed to be mapped into current address
     * space
     */
    return (struct pagedir*)(read_cr3() & 0xFFFFF000);
}

static struct pagetable* get_pagetable(struct va_info info)
{
    /* Pagetables available at (0xFFC00000 + (pde_index * PAGE_SIZE)) because of recursive mapping */
    struct pagetable* table = (struct pagetable*)(0xFFC00000 + (info.dir_index * PAGE_SIZE));
    return table;
}

static void va_info_ex(struct va_info_ex* result, void* va)
{
    assert(paging_enabled);

    memset(result, 0, sizeof(struct va_info_ex));
    result->info = va_info(va);

    /* Get PDE */
    struct pagedir* pagedir = current_pagedir();
    if(pagedir->entries[result->info.dir_index] & PDE_PRESENT) {
        result->pde = pagedir->entries[result->info.dir_index];

        struct pagetable* pagetable = get_pagetable(result->info);
        if(pagetable->entries[result->info.table_index] & PTE_PRESENT) {
            result->pte = pagetable->entries[result->info.table_index];
            result->frame = result->pte & PTE_FRAME;
            result->flags = result->pte & PTE_FLAGS;
        }
    }
}

static void* get_va(unsigned dir_index, unsigned table_index)
{
    const unsigned bytes_per_pde = 4 * 1024 * 1024;
    const unsigned bytes_per_pte = 4 * 1024;
    
    uint32_t result = (dir_index * bytes_per_pde) +
                      (table_index * bytes_per_pte);
    return (void*)result;
}

void vmm_init()
{
    assert(pmm_initialized());

    /* Create initial kernel pagedir */
    struct pagedir* pagedir = kmalloc_a(sizeof(struct pagedir), PAGE_SIZE);
    assert(IS_ALIGNED(pagedir, PAGE_SIZE));
    bzero(pagedir, sizeof(struct pagedir));

    /* 
     * Last entry in pagedir should point to itself so we can modify it when paging is enabled
     * (see recursive mapping)
     */
    assert((uint32_t)pagedir == (((uint32_t)pagedir) & PDE_FRAME));
    pagedir->entries[1023] = ((uint32_t)pagedir) | PDE_PRESENT | PDE_WRITABLE;

    /* Kernel .text */
    for(uint32_t page = (uint32_t)_TEXT_START_; page < (uint32_t)_TEXT_END_; page += PAGE_SIZE) {
        vmm_map_linear(pagedir, page, page, VMM_PAGE_PRESENT);
    }

    /* Kernel .rodata */
    for(uint32_t page = (uint32_t)_RODATA_START_; page < (uint32_t)_RODATA_END_; page += PAGE_SIZE) {
        vmm_map_linear(pagedir, page, page, VMM_PAGE_PRESENT);
    }

    /* Kernel .data */
    for(uint32_t page = (uint32_t)_DATA_START_; page < (uint32_t)_DATA_END_; page += PAGE_SIZE) {
        vmm_map_linear(pagedir, page, page, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
    }

    /* Kernel .bss (also stack, since initial stack is in bss) */
    for(uint32_t page = (uint32_t)_BSS_START_; page < (uint32_t)_BSS_END_; page += PAGE_SIZE) {
        vmm_map_linear(pagedir, page, page, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
    }

    /* Kernel heap */
    struct kernel_heap_info heap_info;
    kernel_heap_info(&heap_info);
    for(uint32_t page = heap_info.heap_start; page < heap_info.heap_start + heap_info.heap_size; page += PAGE_SIZE) {
        vmm_map_linear(pagedir, page, page, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
    }

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

    struct va_info info = va_info((void*)va);

    /* Check if present in page directory */
    if(!(pagedir->entries[info.dir_index] & PDE_PRESENT)) {
        struct pagetable* table = kmalloc_a(sizeof(struct pagetable), PAGE_SIZE);
        assert(IS_ALIGNED((uint32_t)table, PAGE_SIZE));
        bzero(table, sizeof(struct pagetable));

        pagedir->entries[info.dir_index] = ((uint32_t)table) | PDE_PRESENT | PDE_USER | PDE_WRITABLE;
    }

    /* Now check if present in pagetable */
    struct pagetable* table = (struct pagetable*)(pagedir->entries[info.dir_index] & PDE_FRAME);
    if(table->entries[info.table_index] & PTE_PRESENT) {
        trace("VA %p already mapped", va);
        abort();
    }

    table->entries[info.table_index] = (va & PTE_FRAME) | flags;
}

/*
 * Map pages while paging is enabled
 */
void vmm_map(void* va, uint32_t pa, uint32_t flags)
{
    assert(paging_enabled);

    assert(IS_ALIGNED(va, PAGE_SIZE));
    assert(IS_ALIGNED(pa, PAGE_SIZE));

    struct va_info info = va_info((void*)va);

    struct pagedir* pagedir = current_pagedir();

    /* Check if present in page directory */
    bool pde_present = pagedir->entries[info.dir_index] & PDE_PRESENT;
    if(!pde_present) {
        /* Alloc 4k frame for page table */
        uint32_t table_pa = pmm_alloc();

        /* And stash into pagedir */
        pagedir->entries[info.dir_index] = ((uint32_t)table_pa) | PDE_PRESENT | PDE_USER | PDE_WRITABLE;

        /*
         * Reload cr3
         * TODO: Replace with vmm_flush_tlb()
         */
        flush_tlb();

        struct pagetable* table = get_pagetable(info);

        unsigned char* userspace_start = (unsigned char*)(4U * 1024 * 1024);
        if(va == userspace_start) {
            uint32_t* pagedir_view = (uint32_t*)0xFFFFF000;
            trace("pagedir_view[info.dir_index]: 0x%X", pagedir_view[info.dir_index]);
            assert(pagedir_view[info.dir_index] & PDE_PRESENT);
            assert((pagedir_view[info.dir_index] & PDE_FRAME) == table_pa);

            struct va_info_ex vaie;
            va_info_ex(&vaie, table);

            assert(vaie.pde & PDE_PRESENT);
            assert(vaie.pde & PDE_WRITABLE);

            trace("vaie.pte: 0x%X", vaie.pte);
            assert(vaie.pte & PTE_PRESENT);
            assert(vaie.pte & PTE_WRITABLE);
        }

        bzero(table, sizeof(struct pagetable));
        table->entries[info.table_index] = (pa & PTE_FRAME) | flags;
    } else {
        struct pagetable* table = get_pagetable(info);

        if(table->entries[info.table_index] & PTE_PRESENT) {
            trace("VA %p already mapped", va);
            abort();
        }

        table->entries[info.table_index] = (pa & PTE_FRAME) | flags;
    }

    /* TODO: Replace with vmm_flush_tlb */
    flush_tlb();
}

void vmm_remap(void* va, uint32_t flags)
{
    assert(paging_enabled);

    assert(IS_ALIGNED(va, PAGE_SIZE));
    assert(flags & VMM_PAGE_PRESENT);       /* Use vmm_unmap to unmap */

    struct va_info info = va_info((void*)va);

    struct pagedir* pagedir = current_pagedir();

    /* Check if present in page directory */
    bool pde_present = pagedir->entries[info.dir_index] & PDE_PRESENT;
    assert(pde_present);

    /* Pagetables available at (0xFFC00000 + (pte * PAGE_SIZE)) */
    struct pagetable* table = get_pagetable(info);
    assert(table->entries[info.table_index] & PTE_PRESENT);

    /* This will surely fail when we start using other bits of page table entries (e.g. PTE_ACCESSED) */
    uint32_t frame = table->entries[info.table_index] & PTE_FRAME;
    table->entries[info.table_index] = frame | flags;

    flush_tlb();
}

void vmm_flush_tlb(void* va)
{
    invlpg((uint32_t)va);
}

void vmm_unmap(void* va)
{
    assert(paging_enabled);
    assert(IS_ALIGNED(va, PAGE_SIZE));

    struct va_info info = va_info((void*)va);

    struct pagedir* pagedir = current_pagedir();
    bool pde_present = pagedir->entries[info.dir_index] & PDE_PRESENT;
    assert(pde_present);

    struct pagetable* table = get_pagetable(info);
    assert(table->entries[info.table_index] & PTE_PRESENT);

    table->entries[info.table_index] &= ~PTE_PRESENT;

    flush_tlb();
}

bool vmm_paging_enabled()
{
    return paging_enabled;
}

struct pagetable* clone_pagetable(unsigned dir_index)
{
    /* Get pagetable corresponding to address */
    struct va_info info = { .dir_index = dir_index,
                            .table_index = 0 };

    struct pagetable* pagetable = get_pagetable(info);

    /* Create new pagetable */
    struct pagetable* result = kmalloc_a(sizeof(struct pagetable), PAGE_SIZE);
    memset(result, 0, sizeof(struct pagetable));

    /* Create scratch space for pageframe copying */
    void* scratch = kmalloc_a(PAGE_SIZE, PAGE_SIZE);

    struct va_info_ex scratch_info;
    va_info_ex(&scratch_info, scratch);
    vmm_unmap(scratch);

    /* Copy entries */
    for(unsigned table_index = 0; table_index < 1024; table_index++) {
        if(pagetable->entries[table_index] & PTE_PRESENT) {
            unsigned flags = pagetable->entries[table_index] & PTE_FLAGS;

            uint32_t frame = pmm_alloc();
            assert(frame != PMM_INVALID_PAGE);

            /*
             * source page frame contents available in current address space
             * need to map new page frame into current address space for copying
             */
            void* source = get_va(dir_index, table_index);
            void* dest = scratch;

            vmm_map(dest,
                    frame,
                    VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
            memcpy(dest, source, PAGE_SIZE);
            vmm_unmap(dest);

            pagetable->entries[table_index] = frame | flags;
        }
    }

    /* Remap back scratch space */
    vmm_map(scratch, scratch_info.frame, scratch_info.flags);

    return result;
}

struct pagedir* vmm_clone_pagedir()
{
    struct pagedir* pagedir = current_pagedir();

    struct pagedir* result = kmalloc_a(sizeof(struct pagedir), PAGE_SIZE);
    memset(result, 0, sizeof(struct pagedir));

    /*
     * 0x000 - 4Mb:         copy pagedir entry
     * 4Mb   - 3Gb:         copy pagetable entries
     * 3Gb   - end-4Mb:     copy pagedir entry
     * end-4Mb - end:       pagedir address
     */
    result->entries[KERNEL_LO_PDE] = pagedir->entries[KERNEL_LO_PDE];
    for(unsigned i = USER_PDE_START; i <= USER_PDE_END; i++) {
        if(pagedir->entries[i] & PDE_PRESENT) {
            unsigned flags = pagedir->entries[i] & PDE_FLAGS;

            struct pagetable* new_pagetable = clone_pagetable(i);

            struct va_info_ex info;
            va_info_ex(&info, new_pagetable);

            result->entries[i] = info.frame | flags;
        }
    }
    for(int i = KERNEL_HI_PDE_START; i <= KERNEL_HI_PDE_END; i++) {
        result->entries[i] = pagedir->entries[i];
    }
    result->entries[RECURSIVE_MAPPING_PDE] = ((uint32_t)vmm_get_physical(result)) | PDE_PRESENT | PDE_WRITABLE;
    return result;
}

void vmm_switch_pagedir(struct pagedir* pagedir)
{
    assert(IS_ALIGNED(pagedir, PAGE_SIZE));
    uint32_t pa = vmm_get_physical(pagedir);
    write_cr3(pa);
}

uint32_t vmm_get_physical(void* va)
{
    assert(IS_ALIGNED(va, PAGE_SIZE));

    struct va_info_ex info;
    va_info_ex(&info, va);

    return info.frame;
}

uint32_t vmm_get_flags(void* va)
{
    assert(IS_ALIGNED(va, PAGE_SIZE));

    struct va_info_ex info;
    va_info_ex(&info, va);

    return info.flags;
}




