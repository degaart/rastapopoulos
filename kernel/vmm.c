#include "vmm.h"
#include "idt.h"
#include "kmalloc.h"
#include "pmm.h"
#include <debug.h>
#include <string.h>
#include <util.h>

#define DECLARE_SYMBOL(n)        extern unsigned char n[]
#define DECLARE_SECTION_START(n) DECLARE_SYMBOL(_##n##_start)
#define DECLARE_SECTION_END(n)   DECLARE_SYMBOL(_##n##_end)
#define DECLARE_SECTION(n)                                                     \
    DECLARE_SECTION_START(n);                                                  \
    DECLARE_SECTION_END(n)

DECLARE_SECTION(text);
DECLARE_SECTION(rodata);
DECLARE_SECTION(data);
DECLARE_SECTION(user_text);
DECLARE_SECTION(user_data);
DECLARE_SECTION(user_rodata);
DECLARE_SECTION(bss);
DECLARE_SECTION_START(heap);
DECLARE_SYMBOL(_stacktop);

/* Pagetables access */
static struct pagetable* pagetables = (struct pagetable*)0xFFC00000;

/* Pagedirs access */
static struct pagedir* pagedirs = (struct pagedir*)0xFFFFF000;

void vmm_flush()
{
    CLEAR_IF();
    write_cr3(read_cr3());
    RESTORE_IF();
}

bool vmm_frame(uint32_t* frame, void* vaddr)
{
    struct vaddrinfo vi;
    vmm_vaddrinfo(&vi, vaddr);
    if(!vi.pte)
        return false;
    else if(!(*vi.pte & VMM_PRESENT))
        return false;
    *frame = *vi.pte & VMM_FRAME;
    return true;
}

struct pagedir* vmm_create_pagedir()
{
    struct pagedir* pagedir = kvalloc(VMM_ENTRY_COUNT * sizeof(uint32_t));
    memset(pagedir, 0, sizeof(struct pagedir));

    uint32_t frame;
    if(!vmm_frame(&frame, pagedir))
        PANIC("vmm_frame failed");
    return pagedir;
}

void vmm_set_pagedir(struct pagedir* pagedir)
{
    assert(IS_ALIGNED_PTR(pagedir, VMM_PAGESIZE));

    uint32_t frame = 0;
    if(!vmm_frame(&frame, pagedir)) {
        PANIC("Failed to get physical address of %p", pagedir);
    }

    /* Copy kernel mappings */
    CLEAR_IF();
    pagedir->entries[0] = pagedirs->entries[0];
    pagedir->entries[VMM_ENTRY_COUNT - 1] =
        (frame & VMM_FRAME) | VMM_PRESENT | VMM_WRITABLE;
    write_cr3(frame);
    RESTORE_IF();
}

void vmm_vaddrinfo(struct vaddrinfo* info, const void* vaddr)
{
    uintptr_t aligned_vaddr = (uintptr_t)vaddr & ~(VMM_PAGESIZE - 1);
    info->pde_index = aligned_vaddr / (VMM_PAGESIZE * VMM_ENTRY_COUNT);
    info->pte_index =
        (aligned_vaddr % (VMM_PAGESIZE * VMM_ENTRY_COUNT)) / VMM_PAGESIZE;
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
        TRACE("frame 0x%lX is not aligned", frame);
        return false;
    }

    struct vaddrinfo vi;
    vmm_vaddrinfo(&vi, vaddr);
    CLEAR_IF();
    if(*vi.pde & VMM_PRESENT) {
        if(*vi.pte & VMM_PRESENT) {
            TRACE("Address %p already mapped", vaddr);
            RESTORE_IF();
            return false;
        } else {
            *vi.pte = frame | VMM_PRESENT | flags;
            vmm_flush();
            RESTORE_IF();
            return true;
        }
    } else {
        uint32_t pde = pmm_alloc();
        if(pde == INVALID_FRAME) {
            RESTORE_IF();
            return false;
        }

        *vi.pde = pde | VMM_PRESENT | VMM_WRITABLE | VMM_USER;
        vmm_flush();
        memset(pagetables[vi.pde_index].entries, 0, VMM_PAGESIZE);
        pagetables[vi.pde_index].entries[vi.pte_index] =
            frame | VMM_PRESENT | flags;
        vmm_flush();
        RESTORE_IF();
        return true;
    }
}

bool vmm_map_range(const void* vaddr, uint32_t frame, unsigned size,
                   unsigned flags)
{
    const void* vptr;
    uint32_t fptr;
    for(vptr = vaddr, fptr = frame; vptr < vaddr + size;
        vptr += VMM_PAGESIZE, fptr += VMM_PAGESIZE) {
        if(!vmm_map(vptr, fptr, flags)) {
            for(const void* cptr = vaddr; cptr <= vptr; cptr += VMM_PAGESIZE) {
                if(!vmm_unmap(cptr, false))
                    PANIC("vmm_unmap failed");
            }
            return false;
        }
    }
    return true;
}

bool vmm_alloc(const void* vaddr, unsigned flags)
{
    if(!IS_ALIGNED_PTR(vaddr, VMM_PAGESIZE)) {
        TRACE("vaddr %p is not aligned", vaddr);
        return false;
    }

    uint32_t frame = pmm_alloc();
    if(frame == INVALID_FRAME) {
        return false;
    }

    bool ret = vmm_map(vaddr, frame, flags);
    if(!ret) {
        pmm_free(frame);
    }
    return ret;
}

bool vmm_remap(const void* vaddr, unsigned flags)
{
    if(!IS_ALIGNED((uintptr_t)vaddr, VMM_PAGESIZE)) {
        TRACE("vaddr %p is not aligned", vaddr);
        return false;
    } else if((flags & VMM_PRESENT) == 0) {
        TRACE("Invalid vmm_remap flags: 0x%X", flags);
        return false;
    }

    CLEAR_IF();
    struct vaddrinfo vi;
    vmm_vaddrinfo(&vi, vaddr);
    if(!(*vi.pde & VMM_PRESENT)) {
        TRACE("vaddr: pde not present");
        RESTORE_IF();
        return false;
    }
    assert(vi.pte);
    *vi.pte = (*vi.pte & VMM_FRAME) | flags;
    RESTORE_IF();
    return true;
}

bool vmm_unmap(const void* vaddr, bool dealloc_frame)
{
    if(!IS_ALIGNED((uintptr_t)vaddr, VMM_PAGESIZE)) {
        TRACE("vaddr %p is not aligned", vaddr);
        return false;
    }

    CLEAR_IF();
    struct vaddrinfo vi;
    vmm_vaddrinfo(&vi, vaddr);
    if(!(pagedirs->entries[vi.pde_index] & VMM_PRESENT)) {
        TRACE("vaddr %p: pagedir not present", vaddr);
        RESTORE_IF();
        return false;
    } else if(!(pagetables[vi.pde_index].entries[vi.pte_index] & VMM_PRESENT)) {
        TRACE("vaddr %p already unmapped", vaddr);
        RESTORE_IF();
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
            if(dealloc_frame)
                pmm_free(pagedirs->entries[vi.pde_index] & VMM_FRAME);
            pagedirs->entries[vi.pde_index] = 0;
        }
        vmm_flush();
        RESTORE_IF();
        return true;
    }
}

bool vmm_is_readable(void* addr, size_t len)
{
    while(len) {
        struct vaddrinfo vi;
        vmm_vaddrinfo(&vi, (void*)ROUND((uint32_t)addr, VMM_PAGESIZE));

        if(!(pagedirs->entries[vi.pde_index] & VMM_PRESENT)) {
            return false;
        } else if(!(pagetables[vi.pde_index].entries[vi.pte_index] & VMM_PRESENT)) {
            return false;
        }

        addr += VMM_PAGESIZE;
        if(len < VMM_PAGESIZE)
            len = 0;
        else
            len -= VMM_PAGESIZE;
    }
    
    return true;
}

bool vmm_unmap_range(const void* vaddr, unsigned size, bool dealloc_frames)
{
    bool result = true;
    for(const void* ptr = vaddr; ptr < vaddr + size; ptr += VMM_PAGESIZE) {
        if(!vmm_unmap(ptr, false))
            result = false;
    }
    return result;
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
    unsigned dpl;
    isr_t old_pf_handler = idt_handler(0x0E, &dpl);

    idt_add_handler(0x0E, pf_handler, 3);
    pf_count = 0;
    pf_new_eip = &&label1; /* this is a gcc extension */

    uint32_t cr0 = read_cr0();
    assert((cr0 && CR0_WP) != 0);
    static const char* str = "ALL YOUR BASE ARE BELONG TO US";
    uint32_t* ptr = (uint32_t*)str;
    *ptr = 0xDEADBEEF;
label1:
    assert(pf_count == 1);
    idt_add_handler(0x0E, old_pf_handler, dpl);
}

/*
 * Test writes into a non-present page
 */
static void vmm_test_non_present_page()
{
    unsigned dpl;
    isr_t old_pf_handler = idt_handler(0x0E, &dpl);

    idt_add_handler(0x0E, pf_handler, 3);
    pf_count = 0;
    pf_new_eip = &&next;

    uint32_t* ptr = (uint32_t*)0xBADAB000;
    *ptr = 0xDEADBEEF;
next:
    assert(pf_count == 1);
    idt_add_handler(0x0E, old_pf_handler, dpl);
}

/*
 * Test vmm_map
 */
static void vmm_test_map()
{
    unsigned dpl;
    isr_t old_pf_handler = idt_handler(0x0E, &dpl);

    idt_add_handler(0x0E, pf_handler, 3);
    pf_count = 0;
    pf_new_eip = &&failed;

    uint32_t* ptr = (uint32_t*)0xBADAB000;
    uint32_t frame = pmm_alloc();
    if(!vmm_map(ptr, frame, VMM_WRITABLE))
        PANIC("vmm_map failed");
    *ptr = 0xDEADBEEF;
    if(!vmm_unmap(ptr, true))
        PANIC("vmm_unmap failed");
    assert(pf_count == 0);

    idt_add_handler(0x0E, old_pf_handler, dpl);
    return;
failed:
    PANIC("Test failed");
}

/*
 * Test vmm_unmap
 */
static void vmm_test_unmap()
{
    unsigned dpl;
    isr_t old_pf_handler = idt_handler(0x0E, &dpl);

    idt_add_handler(0x0E, pf_handler, 3);
    pf_count = 0;
    pf_new_eip = &&failed;

    uint32_t* ptr = (uint32_t*)0xBADAB000;
    uint32_t frame = pmm_alloc();
    if(!vmm_map(ptr, frame, VMM_WRITABLE))
        PANIC("vmm_map failed");
    *ptr = 0xDEADBEEF;
    if(!vmm_unmap(ptr, true))
        PANIC("vmm_unmap failed");
    pf_new_eip = &&next;
    *ptr = 0x0BADCAFE;
next:
    assert(pf_count == 1);
    idt_add_handler(0x0E, old_pf_handler, dpl);
    return;
failed:
    PANIC("Test failed");
}

/*
 * Test vmm_remap
 */
static void vmm_test_remap()
{
    unsigned dpl;
    isr_t old_pf_handler = idt_handler(0x0E, &dpl);
    idt_add_handler(0x0E, pf_handler, 3);
    pf_count = 0;
    pf_new_eip = &&next;

    uint32_t* ptr = (uint32_t*)0xDEADB000;
    uint32_t frame = pmm_alloc();
    if(!vmm_map(ptr, frame, 0))
        PANIC("vmm_map failed");
    *ptr = 0xDEADBEEF;
failed:
    PANIC("Test failed");
next:
    pf_count = 0;
    pf_new_eip = &&failed;

    if(!vmm_remap(ptr, VMM_PRESENT | VMM_WRITABLE))
        PANIC("vmm_remap failed");
    *ptr = 0xDEADBEEF;
    assert(pf_count == 0);

    if(!vmm_unmap(ptr, true))
        PANIC("vmm_unmap failed");
    idt_add_handler(0x0E, old_pf_handler, dpl);
}

static void idmap_range(struct pagetable* pagetable, const char* name,
                        const void* startp, const void* endp, uint32_t flags)
{
    uint32_t start = ROUND((uint32_t)startp, VMM_PAGESIZE);
    uint32_t end = (uint32_t)endp;
    char info[8] = {0};
    info[0] = (flags & VMM_PRESENT) ? 'R' : ' ';
    info[1] = (flags & VMM_WRITABLE) ? 'W' : ' ';
    info[2] = (flags & VMM_USER) ? 'U' : ' ';
    TRACE("    %-12s 0x%08lX - 0x%08lX [%3s]", name, start, end, info);
    for(uint32_t frame = start; frame <= end; frame += VMM_PAGESIZE) {
        size_t index = frame / VMM_PAGESIZE;
        assert((frame & ~VMM_FRAME) == 0);
        pagetable->entries[index] = frame | flags;
    }
}

void vmm_init(const struct multiboot_mmap_entry* mmap_entries,
              size_t mmap_length)
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
    idmap_range(pagetable, ".text", _text_start, _text_end, VMM_PRESENT);
    idmap_range(pagetable, ".rodata", _rodata_start, _rodata_end, VMM_PRESENT);
    idmap_range(pagetable, ".data", _data_start, _data_end,
                VMM_PRESENT | VMM_WRITABLE);
    idmap_range(pagetable, ".user_text", _user_text_start, _user_text_end,
                VMM_PRESENT | VMM_USER);
    idmap_range(pagetable, ".user_data", _user_data_start, _user_data_end,
                VMM_PRESENT | VMM_WRITABLE | VMM_USER);
    idmap_range(pagetable, ".user_rodata", _user_rodata_start, _user_rodata_end,
                VMM_PRESENT | VMM_USER);
    idmap_range(pagetable, ".bss", _bss_start, _bss_end,
                VMM_PRESENT | VMM_WRITABLE);
    idmap_range(pagetable, "heap", _heap_start, early_kmalloc_get_heap(),
                VMM_PRESENT | VMM_WRITABLE);

    uint32_t stack_guard = (uintptr_t)_stacktop - (VMM_PAGESIZE * 2);
    TRACE("       stack guard  %p []", (void*)stack_guard);

    /* VGA_BASE */
    pagetable->entries[0xB8000 / VMM_PAGESIZE] =
        0xB8000 | VMM_PRESENT | VMM_WRITABLE | VMM_USER;

    /* PDE */
    assert(((uint32_t)pagetable & ~VMM_FRAME) == 0);
    pagedir->entries[0] = ((uint32_t)pagetable & VMM_FRAME) | VMM_PRESENT |
                          VMM_WRITABLE | VMM_USER;

    /* Last entry on pagedir should point to itself */
    pagedir->entries[VMM_ENTRY_COUNT - 1] =
        ((uint32_t)pagedir & VMM_FRAME) | VMM_PRESENT | VMM_WRITABLE;
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
    ADD_TEST(vmm_test_remap);
}

