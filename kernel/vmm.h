#pragma once

#include <stdint.h>

#define VMM_PRESENT             (1 << 0)
#define VMM_WRITABLE            (1 << 1)
#define VMM_USER                (1 << 2)
#define VMM_PWT                 (1 << 3) /* writethrough */
#define VMM_PCD                 (1 << 4) /* cache disable */
#define VMM_ACCESSED            (1 << 5)
#define VMM_AVL                 0xF00
#define VMM_FRAME               0xFFFFF000

#define VMM_PDE_AVL             (1 << 6)
#define VMM_PTE_DIRTY           (1 << 6)

#define VMM_ENTRY_COUNT         1024
#define VMM_PAGESIZE            4096

struct pagedir {
    uint32_t entries[VMM_ENTRY_COUNT];
};

struct pagetable {
    uint32_t entries[VMM_ENTRY_COUNT];
};

struct multiboot_mmap_entry;

struct vaddrinfo {
    size_t pde_index;
    size_t pte_index;
    uint32_t* pde;
    uint32_t* pte;
};

void vmm_vaddrinfo(struct vaddrinfo* info, const void* vaddr);
bool vmm_map(const void* vaddr, uint32_t frame, unsigned flags) __attribute__((warn_unused_result));
bool vmm_remap(const void* vaddr, unsigned flags) __attribute__((warn_unused_result));
bool vmm_unmap(const void* vaddr, bool dealloc_frame) __attribute__((warn_unused_result));
void vmm_init(const struct multiboot_mmap_entry* mmap_entries, size_t mmap_length);


