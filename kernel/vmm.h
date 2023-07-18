#pragma once

#include <stdint.h>

#define VMM_PDE_PRESENT         (1 << 0)
#define VMM_PDE_WRITABLE        (1 << 1)
#define VMM_PDE_USER            (1 << 2)
#define VMM_PDE_PWT             (1 << 3) /* writethrough */
#define VMM_PDE_PCD             (1 << 4) /* cache disable */
#define VMM_PDE_ACCESSED        (1 << 5)
#define VMM_PDE_AVL1            (1 << 6)
#define VMM_PDE_AVL2            0xF00
#define VMM_PDE_PTE             0xFFFFF000  /* physical address of PTE */

#define VMM_PTE_PRESENT         (1 << 0)
#define VMM_PTE_WRITABLE        (1 << 1)
#define VMM_PTE_USER            (1 << 2)
#define VMM_PTE_PWT             (1 << 3) /* writethrough */
#define VMM_PTE_PCD             (1 << 4) /* cache disable */
#define VMM_PTE_ACCESSED        (1 << 5)
#define VMM_PTE_DIRTY           (1 << 6)
#define VMM_PTE_AVL             0xF00
#define VMM_PTE_FRAME           0xFFFFF000

#define VMM_ENTRY_COUNT         1024
#define VMM_PAGESIZE            4096


struct pagedir {
    uint32_t entries[VMM_ENTRY_COUNT];
};

struct pagetable {
    uint32_t entries[VMM_ENTRY_COUNT];
};

void vmm_set_pagedir(const struct pagedir* pagedir);

struct multiboot_mmap_entry;
void vmm_vaddrinfo(const void* vaddr, size_t* pde_index, size_t* pte_index);
bool vmm_map(const void* vaddr, uint32_t frame, unsigned flags) __attribute__((warn_unused_result));
bool vmm_unmap(const void* vaddr) __attribute__((warn_unused_result));
void vmm_init(const struct multiboot_mmap_entry* mmap_entries, size_t mmap_length);


