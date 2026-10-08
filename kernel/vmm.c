#include "vmm.h"
#include "early_malloc.h"
#include "kernel.h"
#include "pmm.h"
#include "stdio.h"

#define ENTRY_COUNT       1024
#define PAGE_MASK         0xfffff000
#define OFFSET_MASK       0x00000fff
#define PAGE_PRESENT      0x001
#define PERMISSION_MASK   (VMM_WRITABLE | VMM_USER)
#define RECURSIVE_INDEX   1023
#define DIRECTORY_ADDRESS 0xfffff000

static bool initialized;

static void flush_tlb(void)
{
    uint32_t cr3 = read_cr3();
    write_cr3(cr3);
}

static volatile uint32_t* directory(void)
{
    return (volatile uint32_t*)(uintptr_t)DIRECTORY_ADDRESS;
}

static volatile uint32_t* table(uint32_t directory_index)
{
    return (volatile uint32_t*)(uintptr_t)(VMM_RECURSIVE_BASE +
                                           directory_index * VMM_PAGE_SIZE);
}

static void clear_page(volatile uint32_t* page)
{
    for (uint32_t i = 0; i < ENTRY_COUNT; ++i)
        page[i] = 0;
}

static bool valid_page_address(uintptr_t address)
{
    return address < VMM_RECURSIVE_BASE && (address & OFFSET_MASK) == 0;
}

static bool valid_flags(uint32_t flags)
{
    return (flags & ~PERMISSION_MASK) == 0;
}

static void* allocate_table_frame(void)
{
    void* frame = pmm_alloc();

    if (frame != 0 && ((uintptr_t)frame & OFFSET_MASK) != 0) {
        pmm_free(frame);
        return 0;
    }

    return frame;
}

static void free_bootstrap_directory(volatile uint32_t* pd)
{
    for (uint32_t i = 0; i < RECURSIVE_INDEX; ++i) {
        if (pd[i] & PAGE_PRESENT)
            pmm_free((void*)(uintptr_t)(pd[i] & PAGE_MASK));
    }

    pmm_free((void*)(uintptr_t)pd);
}

bool vmm_init(void)
{
    uint32_t cr0 = read_cr0();
    uintptr_t start = (uintptr_t)__kernel_start;
    uintptr_t end = (uintptr_t)early_malloc_tail();

    if (initialized || !(cr0 & CR0_PE) || (cr0 & CR0_PG))
        return false;

    if (start >= end || end > VMM_RECURSIVE_BASE)
        return false;

    volatile uint32_t* pd = allocate_table_frame();
    if (pd == 0)
        return false;

    clear_page(pd);

    for (uintptr_t address = start & PAGE_MASK; address < end;
         address += VMM_PAGE_SIZE) {
        uint32_t di = (uint32_t)(address >> 22);
        uint32_t ti = (uint32_t)((address >> 12) & 0x3ff);

        if (!(pd[di] & PAGE_PRESENT)) {
            volatile uint32_t* pt = allocate_table_frame();
            if (pt == 0) {
                free_bootstrap_directory(pd);
                return false;
            }

            clear_page(pt);

            pd[di] = (uint32_t)(uintptr_t)pt | PAGE_PRESENT | VMM_WRITABLE |
                     VMM_USER;
        }

        volatile uint32_t* pt =
            (volatile uint32_t*)(uintptr_t)(pd[di] & PAGE_MASK);

        pt[ti] = (uint32_t)address | PAGE_PRESENT | VMM_WRITABLE;
    }

    pd[RECURSIVE_INDEX] =
        (uint32_t)(uintptr_t)pd | PAGE_PRESENT | VMM_WRITABLE;

    write_cr3((uint32_t)(uintptr_t)pd);
    write_cr0(read_cr0() | CR0_PG);
    initialized = true;
    return true;
}

bool vmm_map(void* frame, void* virtual_address, uint32_t flags)
{
    uintptr_t address = (uintptr_t)virtual_address;
    uintptr_t physical = (uintptr_t)frame;

    if (!initialized || !valid_page_address(address) ||
        (physical & OFFSET_MASK) != 0 || !valid_flags(flags))
        return false;

    uint32_t di = (uint32_t)(address >> 22);
    uint32_t ti = (uint32_t)((address >> 12) & 0x3ff);
    volatile uint32_t* pd = directory();
    volatile uint32_t* pt = table(di);

    if (!(pd[di] & PAGE_PRESENT)) {
        void* new_table = allocate_table_frame();
        if (new_table == 0)
            return false;

        pd[di] = (uint32_t)(uintptr_t)new_table | PAGE_PRESENT | VMM_WRITABLE |
                 VMM_USER;

        flush_tlb();
        clear_page(pt);
    }

    if (pt[ti] & PAGE_PRESENT)
        return false;

    pt[ti] = (uint32_t)physical | PAGE_PRESENT | flags;
    flush_tlb();
    return true;
}

bool vmm_unmap(void* virtual_address)
{
    uintptr_t address = (uintptr_t)virtual_address;

    if (!initialized || !valid_page_address(address))
        return false;

    uint32_t di = (uint32_t)(address >> 22);
    uint32_t ti = (uint32_t)((address >> 12) & 0x3ff);
    volatile uint32_t* pd = directory();

    if (!(pd[di] & PAGE_PRESENT))
        return false;

    volatile uint32_t* pt = table(di);
    if (!(pt[ti] & PAGE_PRESENT))
        return false;

    pt[ti] = 0;

    for (uint32_t i = 0; i < ENTRY_COUNT; ++i) {
        if (pt[i] & PAGE_PRESENT) {
            flush_tlb();
            return true;
        }
    }

    uint32_t table_frame = pd[di] & PAGE_MASK;
    pd[di] = 0;
    flush_tlb();

    pmm_free((void*)(uintptr_t)table_frame);
    return true;
}

bool vmm_remap(void* virtual_address, uint32_t flags)
{
    uintptr_t address = (uintptr_t)virtual_address;

    if (!initialized || !valid_page_address(address) || !valid_flags(flags))
        return false;

    uint32_t di = (uint32_t)(address >> 22);
    uint32_t ti = (uint32_t)((address >> 12) & 0x3ff);

    if (!(directory()[di] & PAGE_PRESENT))
        return false;

    volatile uint32_t* pt = table(di);
    uint32_t entry = pt[ti];
    if (!(entry & PAGE_PRESENT))
        return false;

    pt[ti] = (entry & ~PERMISSION_MASK) | flags;
    flush_tlb();
    return true;
}

bool vmm_frame(const void* virtual_address, void** frame_out)
{
    uintptr_t address = (uintptr_t)virtual_address;

    if (!initialized || frame_out == 0 || address >= VMM_RECURSIVE_BASE)
        return false;

    uint32_t di = (uint32_t)(address >> 22);
    uint32_t ti = (uint32_t)((address >> 12) & 0x3ff);

    if (!(directory()[di] & PAGE_PRESENT))
        return false;

    uint32_t entry = table(di)[ti];
    if (!(entry & PAGE_PRESENT))
        return false;

    *frame_out = (void*)(uintptr_t)(entry & PAGE_MASK);
    return true;
}

