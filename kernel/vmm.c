#include "idt.h"
#include "kmalloc.h"
#include "pmm.h"
#include "vmm.h"
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

void vmm_vaddrinfo(struct vaddrinfo* info, const void* vaddr)
{
    uintptr_t aligned_vaddr = (uintptr_t)vaddr & ~(VMM_PAGESIZE - 1);
    info->pde_index = aligned_vaddr / (VMM_PAGESIZE * VMM_ENTRY_COUNT);
    info->pte_index = (aligned_vaddr % (VMM_PAGESIZE * VMM_ENTRY_COUNT)) / VMM_PAGESIZE;
    info->pde = &pagedirs->entries[info->pde_index];
    if(*info->pde & VMM_PRESENT) {
        info->pte = &pagetables[info->pde_index].entries[info->pte_index];
    } else {
        info->pte = NULL;
    }
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

    struct vaddrinfo vi;
    vmm_vaddrinfo(&vi, vaddr);
    if(*vi.pde & VMM_PRESENT) {
        if(*vi.pte & VMM_PRESENT) {
            return false;
        } else {
            *vi.pte = frame | VMM_PRESENT | flags;
            vmm_flush();
            return true;
        }
    } else {
        *vi.pde = pmm_alloc() | VMM_PRESENT | VMM_WRITABLE | VMM_USER;
        vmm_flush();
        memset(pagetables[vi.pde_index].entries, 0, VMM_PAGESIZE);
        pagetables[vi.pde_index].entries[vi.pte_index] =
            frame | VMM_PRESENT | flags;
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

    struct vaddrinfo vi;
    vmm_vaddrinfo(&vi, vaddr);
    if(!(pagedirs->entries[vi.pde_index] & VMM_PRESENT)) {
        TRACE("vaddr %p: pagedir not present", vaddr);
        return false;
    } else if(!(pagetables[vi.pde_index].entries[vi.pte_index] & VMM_PRESENT)) {
        TRACE("vaddr %p already unmapped", vaddr);
        return false;
    } else {
        pagetables[vi.pde_index].entries[vi.pte_index] = 0;

        /*
         * If no more pages are present in this PDE, free it
         */
        size_t i;
        for(i = 0; i < VMM_ENTRY_COUNT; i++) {
            if(pagetables[vi.pde_index].entries[i] & VMM_PRESENT) {
                break;
            }
        }

        if(i == VMM_ENTRY_COUNT) {
            pmm_free(pagedirs->entries[vi.pde_index] & VMM_FRAME);
            pagedirs->entries[vi.pde_index] = 0;
        }
        vmm_flush();
        return true;
    }
}

static unsigned pf_count = 0;
static void* pf_new_eip = NULL;
static void pf_handler(struct isr_regs* regs)
{
    pf_count++;
    if(pf_new_eip)
        regs->eip = (uint32_t)pf_new_eip;
}


/*
 * Testing write into RO page (int 0x0E page fault, only with CR0.WP)
 * Should trigger an int 0x0E page fault
 * But only works with CR0.WP and 486+
 */
static void vmm_test_ro_page()
{
    idt_add_handler(0x0E, pf_handler, 3);
    pf_count = 0;
    pf_new_eip = &&label1;          /* this is a gcc extension */

    uint32_t cr0 = read_cr0();
    assert((cr0 && CR0_WP) != 0);
    static const unsigned char* str = "ALL YOUR BASE ARE BELONG TO US";
    uint32_t* ptr = (uint32_t*)str;
    *ptr = 0xDEADBEEF;
label1:
    assert(pf_count == 1);
}

/*
 * Test writes into a non-present page
 */
static void vmm_test_non_present_page()
{
    idt_add_handler(0x0E, pf_handler, 3);
    pf_count = 0;
    pf_new_eip = &&next;

    uint32_t* ptr = (uint32_t*)0xBADAB000;
    *ptr = 0xDEADBEEF;
next:
    assert(pf_count == 1);
}

/*
 * Test vmm_map
 */
static void vmm_test_map()
{
    idt_add_handler(0x0E, pf_handler, 3);
    pf_count = 0;
    pf_new_eip = &&failed;

    uint32_t* ptr = (uint32_t*)0xBADAB000;
    uint32_t frame = pmm_alloc();
    if(!vmm_map(ptr, frame, VMM_WRITABLE))
        PANIC("vmm_map failed");
    *ptr = 0xDEADBEEF;
    if(!vmm_unmap(ptr))
        PANIC("vmm_unmap failed");
    assert(pf_count == 0);
    return;
failed:
    PANIC("Test failed");
}

/*
 * Test vmm_unmap
 */
static void vmm_test_unmap()
{
    idt_add_handler(0x0E, pf_handler, 3);
    pf_count = 0;
    pf_new_eip = &&failed;

    uint32_t* ptr = (uint32_t*)0xBADAB000;
    uint32_t frame = pmm_alloc();
    if(!vmm_map(ptr, frame, VMM_WRITABLE))
        PANIC("vmm_map failed");
    *ptr = 0xDEADBEEF;
    if(!vmm_unmap(ptr))
        PANIC("vmm_unmap failed");
    pf_new_eip = &&next;
    *ptr = 0x0BADCAFE;
next:
    assert(pf_count == 1);
    return;
failed:
    PANIC("Test failed");
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
        assert((frame & ~VMM_FRAME) == 0);
        pagetable->entries[index] = frame | VMM_PRESENT;
    }

    start = ROUND((uint32_t)_rodata_start, VMM_PAGESIZE);
    end = (uint32_t)_rodata_end;
    TRACE("    .rodata %p - %p [R]", start, end);
    for(uint32_t frame = start; frame <= end; frame += VMM_PAGESIZE) {
        size_t index = frame / VMM_PAGESIZE;
        assert((frame & ~VMM_FRAME) == 0);
        pagetable->entries[index] = frame | VMM_PRESENT;
    }

    start = ROUND((uint32_t)_data_start, VMM_PAGESIZE);
    end = (uint32_t)_data_end;
    TRACE("    .data   %p - %p [RW]", start, end);
    for(uint32_t frame = start; frame <= end; frame += VMM_PAGESIZE) {
        size_t index = frame / VMM_PAGESIZE;
        assert((frame & ~VMM_FRAME) == 0);
        pagetable->entries[index] = frame | VMM_PRESENT | VMM_WRITABLE;
    }

    start = ROUND((uint32_t)_bss_start, VMM_PAGESIZE);
    end = (uint32_t)_bss_end;
    uint32_t stack_guard = (uintptr_t)_stacktop - (VMM_PAGESIZE * 2);
    TRACE("    .bss    %p - %p [RW]", start, end);
    for(uint32_t frame = start; frame <= end; frame += VMM_PAGESIZE) {
        if(frame != stack_guard) {
            size_t index = frame / VMM_PAGESIZE;
            assert((frame & ~VMM_FRAME) == 0);
            pagetable->entries[index] = frame | VMM_PRESENT | VMM_WRITABLE;
        }
    }

    start = ROUND((uint32_t)_heap_start, VMM_PAGESIZE);
    end = (uint32_t)early_kmalloc_get_heap();
    TRACE("     heap   %p - %p [RW]", start, end);
    for(uint32_t frame = start; frame <= end; frame += VMM_PAGESIZE) {
        size_t index = frame / VMM_PAGESIZE;
        assert((frame & ~VMM_FRAME) == 0);
        pagetable->entries[index] = frame | VMM_PRESENT | VMM_WRITABLE;
    }
    TRACE("     stack guard: %p []", stack_guard);

    /* Map VGA_BASE as we need it for debugging */
    pagetable->entries[0xB8000/VMM_PAGESIZE] = 0xB8000 | VMM_PRESENT | VMM_WRITABLE;


    assert(((uint32_t)pagetable & ~VMM_FRAME) == 0);
    pagedir->entries[0] = ((uint32_t)pagetable & VMM_FRAME) |
                          VMM_PRESENT |
                          VMM_WRITABLE;

    /* Last entry on pagedir should point to itself */
    pagedir->entries[VMM_ENTRY_COUNT-1] = ((uint32_t)pagedir & VMM_FRAME) |
                                          VMM_PRESENT |
                                          VMM_WRITABLE;
    write_cr3((uint32_t)pagedir);

    uint32_t cr0 = read_cr0();
    cr0 |= CR0_PG | CR0_WP;
    write_cr0(cr0);

    /* Tests (some are only run on a 486+ */
    if(!is_386())
        ADD_TEST(vmm_test_ro_page);
    ADD_TEST(vmm_test_non_present_page);
    ADD_TEST(vmm_test_map);
    ADD_TEST(vmm_test_unmap);
}

