#include "kmalloc.h"
#include "vmm.h"
#include "pmm.h"
#include <debug.h>
#include <string.h>
#include <util.h>

extern unsigned char _text_start[];
extern unsigned char _text_end[];
extern unsigned char _rodata_start[];
extern unsigned char _rodata_end[];
extern unsigned char _data_start[];
extern unsigned char _data_end[];
extern unsigned char _bss_start[];
extern unsigned char _bss_end[];
extern unsigned char _heap_start[];
extern unsigned char _stacktop[];

/* Pagetables access */
static struct pagetable* pagetables = (struct pagetable*)0xFFC00000;

/* Pagedirs access */
static struct pagedir* pagedirs = (struct pagedir*)0xFFFFF000;

void vmm_flush()
{
    write_cr3(read_cr3());
}

void vmm_vaddrinfo(const void* vaddr, size_t* pde_index, size_t* pte_index)
{
    uintptr_t aligned_vaddr = (uintptr_t)vaddr & ~(VMM_PAGESIZE - 1);
    *pde_index = aligned_vaddr / (VMM_PAGESIZE * VMM_ENTRY_COUNT);
    *pte_index = (aligned_vaddr % (VMM_PAGESIZE * VMM_ENTRY_COUNT)) / VMM_PAGESIZE;
}

bool vmm_map(const void* vaddr, uint32_t frame, unsigned flags)
{
    if(!IS_ALIGNED((uintptr_t)vaddr, VMM_PAGESIZE)) {
        TRACE("vaddr %p is not aligned", vaddr);
        return false;
    } else if(!IS_ALIGNED(frame, VMM_PAGESIZE)) {
        TRACE("frame %p is not aligned", frame);
        return false;
    }

    size_t pde_index, pte_index;
    vmm_vaddrinfo(vaddr, &pde_index, &pte_index);
    if(pagedirs->entries[pde_index] & VMM_PDE_PRESENT) {
        if(pagetables[pde_index].entries[pte_index] & VMM_PTE_PRESENT) {
            return false;
        } else {
            pagetables[pde_index].entries[pte_index] = 
                frame | VMM_PTE_PRESENT | flags;
            vmm_flush();
            return true;
        }
    } else {
        pagedirs->entries[pde_index] =
            pmm_alloc() |
            VMM_PDE_PRESENT |
            VMM_PDE_WRITABLE |
            VMM_PDE_USER;
        vmm_flush();
        memset(pagetables[pde_index].entries, 0, VMM_PAGESIZE);
        pagetables[pde_index].entries[pte_index] =
            frame | VMM_PTE_PRESENT | flags;
        vmm_flush();
        return true;
    }
}

bool vmm_unmap(const void* vaddr)
{
    if(!IS_ALIGNED((uintptr_t)vaddr, VMM_PAGESIZE)) {
        TRACE("vaddr %p is not aligned", vaddr);
        return false;
    }

    size_t pde_index, pte_index;
    vmm_vaddrinfo(vaddr, &pde_index, &pte_index);
    if(!(pagedirs->entries[pde_index] & VMM_PDE_PRESENT)) {
        TRACE("vaddr %p: pagedir not present", vaddr);
        return false;
    } else if(!(pagetables[pde_index].entries[pte_index] & VMM_PTE_PRESENT)) {
        TRACE("vaddr %p already unmapped", vaddr);
        return false;
    } else {
        pagetables[pde_index].entries[pte_index] = 0;

        /*
         * If no more pages are present in this PDE, free it
         */
        size_t i;
        for(i = 0; i < VMM_ENTRY_COUNT; i++) {
            if(pagetables[pde_index].entries[i] & VMM_PTE_PRESENT) {
                break;
            }
        }

        if(i == VMM_ENTRY_COUNT) {
            pmm_free(pagedirs->entries[pde_index] & VMM_PDE_PTE);
            pagedirs->entries[pde_index] = 0;
        }
        vmm_flush();
        return true;
    }
}

static void vmm_test()
{


}

void vmm_init(const struct multiboot_mmap_entry* mmap_entries, size_t mmap_length)
{
    /*
     * After our call to pmm_alloc, early_kmalloc would return invalid results,
     * so we must disable it
     */
    early_kmalloc_enabled = false;
    struct pagedir* pagedir = (struct pagedir*)pmm_alloc();
    TRACE("pagedir: %p", pagedir);
    memset(pagedir, 0, sizeof(struct pagedir));

    /*
     * Identity-map kernel region
     */
    struct pagetable* pagetable = (struct pagetable*)pmm_alloc();
    TRACE("pagetable: %p", pagetable);
    memset(pagetable, 0, sizeof(struct pagetable));

    TRACE("Kernel mappings:");

    uint32_t start = ROUND((uint32_t)_text_start, VMM_PAGESIZE);
    uint32_t end = (uint32_t)_text_end;
    TRACE("    .text   %p - %p [R]", start, end);
    for(uint32_t frame = start; frame <= end; frame += VMM_PAGESIZE) {
        size_t index = frame / VMM_PAGESIZE;
        assert((frame & ~VMM_PTE_FRAME) == 0);
        pagetable->entries[index] = frame | VMM_PTE_PRESENT;
    }

    start = ROUND((uint32_t)_rodata_start, VMM_PAGESIZE);
    end = (uint32_t)_rodata_end;
    TRACE("    .rodata %p - %p [R]", start, end);
    for(uint32_t frame = start; frame <= end; frame += VMM_PAGESIZE) {
        size_t index = frame / VMM_PAGESIZE;
        assert((frame & ~VMM_PTE_FRAME) == 0);
        pagetable->entries[index] = frame | VMM_PTE_PRESENT;
    }

    start = ROUND((uint32_t)_data_start, VMM_PAGESIZE);
    end = (uint32_t)_data_end;
    TRACE("    .data   %p - %p [RW]", start, end);
    for(uint32_t frame = start; frame <= end; frame += VMM_PAGESIZE) {
        size_t index = frame / VMM_PAGESIZE;
        assert((frame & ~VMM_PTE_FRAME) == 0);
        pagetable->entries[index] = frame | VMM_PTE_PRESENT | VMM_PTE_WRITABLE;
    }

    start = ROUND((uint32_t)_bss_start, VMM_PAGESIZE);
    end = (uint32_t)_bss_end;
    uint32_t stack_guard = (uintptr_t)_stacktop - (VMM_PAGESIZE * 2);
    TRACE("    .bss    %p - %p [RW]", start, end);
    for(uint32_t frame = start; frame <= end; frame += VMM_PAGESIZE) {
        if(frame != stack_guard) {
            size_t index = frame / VMM_PAGESIZE;
            assert((frame & ~VMM_PTE_FRAME) == 0);
            pagetable->entries[index] = frame | VMM_PTE_PRESENT | VMM_PTE_WRITABLE;
        }
    }

    start = ROUND((uint32_t)_heap_start, VMM_PAGESIZE);
    end = (uint32_t)early_kmalloc_get_heap();
    TRACE("     heap   %p - %p [RW]", start, end);
    for(uint32_t frame = start; frame <= end; frame += VMM_PAGESIZE) {
        size_t index = frame / VMM_PAGESIZE;
        assert((frame & ~VMM_PTE_FRAME) == 0);
        pagetable->entries[index] = frame | VMM_PTE_PRESENT | VMM_PTE_WRITABLE;
    }
    TRACE("     stack guard: %p []", stack_guard);

    /* Map VGA_BASE as we need it for debugging */
    pagetable->entries[0xB8000/VMM_PAGESIZE] = 0xB8000 | VMM_PTE_PRESENT | VMM_PTE_WRITABLE;


    assert(((uint32_t)pagetable & ~VMM_PDE_PTE) == 0);
    pagedir->entries[0] = ((uint32_t)pagetable & VMM_PDE_PTE) |
                          VMM_PDE_PRESENT |
                          VMM_PDE_WRITABLE;

    /* Last entry on pagedir should point to itself */
    pagedir->entries[VMM_ENTRY_COUNT-1] = ((uint32_t)pagedir & VMM_PDE_PTE) |
                                          VMM_PDE_PRESENT |
                                          VMM_PDE_WRITABLE;
    write_cr3((uint32_t)pagedir);

    uint32_t cr0 = read_cr0();
    cr0 |= CR0_PG | CR0_WP;
    write_cr0(cr0);
    add_test("vmm", vmm_test);
}

