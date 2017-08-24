#include "vmm.h"
#include "kdebug.h"
#include "pmm.h"
#include "kmalloc.h"
#include "registers.h"
#include "string.h"
#include "util.h"
#include "kernel.h"
#include "locks.h"
#include "list.h"

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

#define USER_PDE_START                  0
#define USER_PDE_END                    (PAGE_DIRECTORY_INDEX(KERNEL_START) - 1)
#define KERNEL_PDE_START                PAGE_DIRECTORY_INDEX(KERNEL_START)
#define KERNEL_PDE_END                  1022
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
static struct pagedir*  current_pagedir = (struct pagedir*)0xFFFFF000;
static struct pagedir*  current_pagedir_va = NULL;

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
    enter_critical_section();
    write_cr3(read_cr3());
    leave_critical_section();
}

/* 
 * TODO: Unpack parameters, some calling code dont have a va_info
 * Still require two params so we cannot possibly be confused on wether
 * to pass a dir index or a table index
 */
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
    if(current_pagedir->entries[result->info.dir_index] & PDE_PRESENT) {
        result->pde = current_pagedir->entries[result->info.dir_index];

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
    pagedir->entries[1023] = (((uint32_t)pagedir) - KERNEL_BASE_ADDR) | PDE_PRESENT | PDE_WRITABLE;

    /* Kernel .text */
    for(uint32_t page = (uint32_t)_TEXT_START_; page < (uint32_t)_TEXT_END_; page += PAGE_SIZE) {
        vmm_map_linear(pagedir, page, page - KERNEL_BASE_ADDR, VMM_PAGE_PRESENT);
    }

    /* Kernel .rodata */
    for(uint32_t page = (uint32_t)_RODATA_START_; page < (uint32_t)_RODATA_END_; page += PAGE_SIZE) {
        vmm_map_linear(pagedir, page, page - KERNEL_BASE_ADDR, VMM_PAGE_PRESENT | VMM_PAGE_USER); // TODO: Remove user mapping
    }

    /* Kernel .data */
    for(uint32_t page = (uint32_t)_DATA_START_; page < (uint32_t)_DATA_END_; page += PAGE_SIZE) {
        vmm_map_linear(pagedir, page, page - KERNEL_BASE_ADDR, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
    }

    /* Kernel .bss (also stack, since initial stack is in bss) */
    for(uint32_t page = (uint32_t)_BSS_START_; page < (uint32_t)_BSS_END_; page += PAGE_SIZE) {
        vmm_map_linear(pagedir, page, page - KERNEL_BASE_ADDR, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
    }

    /* Kernel heap */
    struct kernel_heap_info heap_info;
    kernel_heap_info(&heap_info);
    for(uint32_t page = heap_info.heap_start; page < heap_info.heap_start + heap_info.heap_size; page += PAGE_SIZE) {
        vmm_map_linear(pagedir, page, page - KERNEL_BASE_ADDR, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
    }

    /* Multiboot heap */
    struct heap_info multiboot_heap = multiboot_heap_info();
    for(uint32_t page = (uint32_t)multiboot_heap.address;
        page < (uint32_t)multiboot_heap.address + multiboot_heap.size; 
        page += PAGE_SIZE) {
        vmm_map_linear(pagedir, page, page - KERNEL_BASE_ADDR, VMM_PAGE_PRESENT);
    }

    current_pagedir_va = pagedir;

    /* Enable paging */
    uint32_t pagedir_pa = ((uint32_t)pagedir) - KERNEL_BASE_ADDR;
    write_cr3(pagedir_pa);

    uint32_t cr0 = read_cr0();
    cr0 |= CR0_PG | CR0_WP; /* CR0_WP: ring0 cannot write to write-protected pages */
    write_cr0(cr0);

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

        pagedir->entries[info.dir_index] = (((uint32_t)table) - KERNEL_BASE_ADDR) | PDE_PRESENT | PDE_USER | PDE_WRITABLE;
    }

    /* Now check if present in pagetable */
    struct pagetable* table = (struct pagetable*)((pagedir->entries[info.dir_index] & PDE_FRAME) + KERNEL_BASE_ADDR);
    if(table->entries[info.table_index] & PTE_PRESENT) {
        trace("VA %p already mapped", va);
        abort();
    }

    table->entries[info.table_index] = (pa & PTE_FRAME) | flags;
}

/*
 * Map pages using another pagedir
 */
void vmm_pagedir_map(struct pagedir* pagedir, void* va, uint32_t pa, uint32_t flags)
{
    panic("Bad code");

    assert(paging_enabled);

    assert(flags & VMM_PAGE_PRESENT);
    assert(IS_ALIGNED(va, PAGE_SIZE));
    assert(IS_ALIGNED(pa, PAGE_SIZE));

    struct va_info info = va_info((void*)va);

    /* Check if present in page directory */
    bool pde_present = pagedir->entries[info.dir_index] & PDE_PRESENT;
    if(!pde_present) {
        /*
         * NOTE: Do not call kmalloc in this function as kmalloc
         * might call vmm_map
         *
         * Alloc 4k frame for page table
         */
        uint32_t table_pa = pmm_alloc();

        /* And stash into pagedir */
        pagedir->entries[info.dir_index] = ((uint32_t)table_pa) | PDE_PRESENT | PDE_USER | PDE_WRITABLE;

        struct pagetable* table = vmm_transient_map(table_pa, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
        bzero(table, sizeof(struct pagetable));

        table->entries[info.table_index] = (pa & PTE_FRAME) | flags;

        vmm_transient_unmap(table);
    } else {
        uint32_t table_pa = pagedir->entries[info.dir_index] & PDE_FRAME;
        struct pagetable* table = vmm_transient_map(table_pa, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);

        if(table->entries[info.table_index] & PTE_PRESENT) {
            trace("VA %p already mapped", va);
            abort();
        }

        table->entries[info.table_index] = (pa & PTE_FRAME) | flags;
        vmm_transient_unmap(table);
    }
}

/*
 * Map pages while paging is enabled
 */
void vmm_map(void* va, uint32_t pa, uint32_t flags)
{
    assert(paging_enabled);

    assert(flags & VMM_PAGE_PRESENT);
    assert(IS_ALIGNED(va, PAGE_SIZE));
    assert(IS_ALIGNED(pa, PAGE_SIZE));

    enter_critical_section();

    struct va_info info = va_info((void*)va);

    /* Check if present in page directory */
    bool pde_present = current_pagedir->entries[info.dir_index] & PDE_PRESENT;
    if(!pde_present) {
        /*
         * NOTE: Do not call kmalloc in this function as kmalloc
         * might call vmm_map
         *
         * Alloc 4k frame for page table
         */
        uint32_t table_pa = pmm_alloc();

        /* And stash into pagedir */
        current_pagedir->entries[info.dir_index] = ((uint32_t)table_pa) | PDE_PRESENT | PDE_USER | PDE_WRITABLE;

        /*
         * Reload cr3
         * TODO: Replace with vmm_flush_tlb()
         */
        flush_tlb();

        pde_present = current_pagedir->entries[info.dir_index] & PDE_PRESENT;
        assert(pde_present);

        struct pagetable* table = get_pagetable(info);
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

    leave_critical_section();
}

void vmm_remap(void* va, uint32_t flags)
{
    assert(paging_enabled);

    assert(IS_ALIGNED(va, PAGE_SIZE));
    assert(flags & VMM_PAGE_PRESENT);       /* Use vmm_unmap to unmap */

    enter_critical_section();

    struct va_info info = va_info((void*)va);

    /* Check if present in page directory */
    bool pde_present = current_pagedir->entries[info.dir_index] & PDE_PRESENT;
    assert(pde_present);

    /* Pagetables available at (0xFFC00000 + (pte * PAGE_SIZE)) */
    struct pagetable* table = get_pagetable(info);
    assert(table->entries[info.table_index] & PTE_PRESENT);

    /* This will surely fail when we start using other bits of page table entries (e.g. PTE_ACCESSED) */
    uint32_t frame = table->entries[info.table_index] & PTE_FRAME;
    table->entries[info.table_index] = frame | flags;

    flush_tlb();

    leave_critical_section();
}

void vmm_flush_tlb(void* va)
{
    invlpg((uint32_t)va);
}

void vmm_unmap(void* va)
{
    assert(paging_enabled);
    assert(IS_ALIGNED(va, PAGE_SIZE));

    enter_critical_section();

    struct va_info info = va_info((void*)va);

    bool pde_present = current_pagedir->entries[info.dir_index] & PDE_PRESENT;
    assert(pde_present);

    struct pagetable* table = get_pagetable(info);
    assert(table->entries[info.table_index] & PTE_PRESENT);

    table->entries[info.table_index] &= ~PTE_PRESENT;

    flush_tlb();

    leave_critical_section();
}

bool vmm_paging_enabled()
{
    return paging_enabled;
}

static void clone_pagetable(struct pagetable* dst, struct pagetable* src)
{
    bzero(dst, sizeof(struct pagetable));

    for(unsigned i = 0; i < 1024; i++) {
        if(src->entries[i] & PTE_PRESENT) {
            uint32_t frame = src->entries[i] & PTE_FRAME;
            uint32_t flags = src->entries[i] & PTE_FLAGS;

            uint32_t new_frame = pmm_alloc();

            /* TODO: No need map for source buf */
            void* src_buf = vmm_transient_map(frame, VMM_PAGE_PRESENT);
            void* dst_buf = vmm_transient_map(new_frame, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
            memcpy(dst_buf, src_buf, PAGE_SIZE);
            vmm_transient_unmap(dst_buf);
            vmm_transient_unmap(src_buf);

            dst->entries[i] = new_frame | flags;
        }
    }
}

struct pagedir* vmm_clone_pagedir()
{
    struct pagedir* result = kmalloc_a(sizeof(struct pagedir), PAGE_SIZE);
    memset(result, 0, sizeof(struct pagedir));

    enter_critical_section();

    /*
     * 0     - 3Gb:         copy pagetable entries
     * 3Gb   - end-4Mb:     copy pagedir entry
     * end-4Mb - end:       pagedir address
     */
    for(unsigned i = USER_PDE_START; i <= USER_PDE_END; i++) {
        if(current_pagedir->entries[i] & PDE_PRESENT) {
            struct va_info info = {
                .dir_index = i,
                .table_index = 0
            };

            struct pagetable* src = get_pagetable(info);

            uint32_t dst_frame = pmm_alloc();
            struct pagetable* dst = vmm_transient_map(dst_frame, VMM_PAGE_PRESENT|VMM_PAGE_WRITABLE);

            clone_pagetable(dst, src);

            vmm_transient_unmap(dst);

            uint32_t flags = current_pagedir->entries[i] & PDE_FLAGS;
            result->entries[i] = dst_frame | flags;
        }
    }
    for(int i = KERNEL_PDE_START; i <= KERNEL_PDE_END; i++) {
        result->entries[i] = current_pagedir->entries[i];
    }
    result->entries[RECURSIVE_MAPPING_PDE] = ((uint32_t)vmm_get_physical(result)) | PDE_PRESENT | PDE_WRITABLE;

    leave_critical_section();
    return result;
}

void vmm_destroy_pagedir(struct pagedir* pagedir)
{
    for(unsigned i = USER_PDE_START; i <= USER_PDE_END; i++) {
        if(pagedir->entries[i] & PDE_PRESENT) {
            uint32_t table_frame = pagedir->entries[i] & PDE_FRAME;

            struct pagetable* table = vmm_transient_map(table_frame,
                                                        VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);

            for(unsigned i = 0; i < 1024; i++) {
                if(table->entries[i] & PTE_PRESENT) {
                    uint32_t frame = table->entries[i] & PTE_FRAME;
                    pmm_free(frame);
                }
            }

            vmm_transient_unmap(table);
            pmm_free(table_frame);
        }
    }

    kfree(pagedir);
}

uint32_t vmm_get_physical(void* va)
{
    assert(IS_ALIGNED(va, PAGE_SIZE));

    enter_critical_section();

    struct va_info_ex info;
    va_info_ex(&info, va);

    leave_critical_section();

    return info.frame;
}

uint32_t vmm_get_flags(void* va)
{
    assert(IS_ALIGNED(va, PAGE_SIZE));

    enter_critical_section();

    struct va_info_ex info;
    va_info_ex(&info, va);

    leave_critical_section();

    return info.flags;
}

struct transient_map {
    uint32_t frame;
    unsigned flags;
    void* address;
    list_declare_node(transient_map) node;
};
list_declare(transient_map_list, transient_map);
static struct transient_map_list transient_maps;

void* vmm_transient_map(uint32_t frame, unsigned flags)
{
    void* address = kmalloc_a(PAGE_SIZE, PAGE_SIZE);

    enter_critical_section();

    struct transient_map* map = kmalloc(sizeof(struct transient_map));
    bzero(map, sizeof(struct transient_map));
    map->frame = vmm_get_physical(address);
    map->flags = vmm_get_flags(address);
    map->address = address;

    vmm_unmap(address);
    vmm_map(address, frame, VMM_PAGE_PRESENT | flags);

    list_append(&transient_maps, map, node);

    leave_critical_section();

    return address;
}

void vmm_transient_unmap(void* address)
{
    enter_critical_section();

    bool found = false;
    list_foreach(transient_map, map, &transient_maps, node) {
        if(map->address == address) {
            vmm_unmap(address);
            vmm_map(address, map->frame, map->flags);

            list_remove(&transient_maps, map, node);
            
            kfree(map);
            kfree(address);
            found = true;

            break;
        }
    }

    if(!found) {
        panic("Invalid transient map address");
    }
    
    leave_critical_section();
}

struct pagedir* vmm_current_pagedir()
{
    assert(paging_enabled);

    /* 
     * NOTE: cr3() is the physical address of the pagedir, not it's virtual address!
     * We cant use 0xFFFFF000 either because calling code might save current pagedir and reuse it
     */
    return current_pagedir_va;
}

void vmm_switch_pagedir(struct pagedir* pagedir)
{
    assert(IS_ALIGNED(pagedir, PAGE_SIZE));

    vmm_copy_kernel_mappings(pagedir);

    enter_critical_section();
    uint32_t pa = vmm_get_physical(pagedir);
    write_cr3(pa);
    current_pagedir_va = pagedir;
    leave_critical_section();
}

void vmm_copy_kernel_mappings(struct pagedir* pagedir)
{
    assert(IS_ALIGNED(pagedir, PAGE_SIZE));

    enter_critical_section();
    
    /*
     * Copy kernel mappings into new pagedir
     */
    for(int i = KERNEL_PDE_START; i <= KERNEL_PDE_END; i++) {
        pagedir->entries[i] = current_pagedir->entries[i];
    }

    uint32_t pa = vmm_get_physical(pagedir);
    assert((pagedir->entries[RECURSIVE_MAPPING_PDE] & PDE_FRAME) == pa);
    assert(pagedir->entries[RECURSIVE_MAPPING_PDE] & PDE_PRESENT);
    assert(pagedir->entries[RECURSIVE_MAPPING_PDE] & PDE_WRITABLE);

    leave_critical_section();
}

