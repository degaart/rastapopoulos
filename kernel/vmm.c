#include "vmm.h"
#include "pmm.h"
#include "debug.h"
#include "string.h"
#include "kmalloc.h"
#include "idt.h"
#include "registers.h"
#include "halt.h"
#include "kernel.h"

#define ENTRY(flags, addr) ((flags) | (((uint32_t)(addr)) & PTE_FRAME))

#define PDE_INDEX(addr) (((unsigned long)(addr)) >> 22)
#define PTE_INDEX(addr) ((((unsigned long)(addr)) >> 12) & 0x03FF)
#define CURRENT_PAGEDIR 0xFFFFF000
#define CURRENT_PAGETABLES 0xFFC00000

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

#define CR0_PG  (1 << 31)
#define CR0_CD  (1 << 30)
#define CR0_NW  (1 << 29)
#define CR0_AM  (1 << 18)
#define CR0_WP  (1 << 16)
#define CR0_NE  (1 << 5)
#define CR0_ET  (1 << 4)
#define CR0_TS  (1 << 3)
#define CR0_EM  (1 << 2)
#define CR0_MP  (1 << 1)
#define CR0_PE  (1)


static bool is_initialized = false;

static
uint32_t get_pde(void* page)
{
    uint32_t* pagedir = (uint32_t*)CURRENT_PAGEDIR;
    return pagedir[PDE_INDEX(page)];
}

static
void set_pde(void* page, uint32_t pde)
{
    uint32_t* pagedir = (uint32_t*)CURRENT_PAGEDIR;
    pagedir[PDE_INDEX(page)] = pde;
}

static
uint32_t get_pte(void* page)
{
    uint32_t* pagetable = ((uint32_t*)CURRENT_PAGETABLES) + (0x400 * PDE_INDEX(page));
    return pagetable[PTE_INDEX(page)];
}

static
void set_pte(void* page, uint32_t pte)
{
    uint32_t* pagetable = ((uint32_t*)CURRENT_PAGETABLES) + (0x400 * PDE_INDEX(page));
    pagetable[PTE_INDEX(page)] = pte;
}

static void page_fault_handler(const struct isr_regs* regs)
{
    unsigned long address = read_cr2();
    bool present = !(regs->err_code & 0x01);
    bool write = regs->err_code & 0x02;
    bool usermode = regs->err_code & 0x04;
    bool reserved = regs->err_code & 0x08;
    bool ins_fetch = regs->err_code & 0x10;

    char status[6] = { 0 };
    status[0] = present ? 'P' : 'p';
    status[1] = write ? 'W' : 'w';
    status[2] = usermode ? 'U' : 'u';
    status[3] = reserved ? 'R' : 'r';
    status[4] = ins_fetch ? 'I' : 'i';

    panic("Page fault\n"
          "\tAddress: %p\n"
          "\tStatus: %s\n"
          "\tCS: %p DS: %p EIP: %p ESP: %p",
          address,
          status,
          regs->cs, regs->ds, regs->eip, regs->esp);
}

void vmm_init()
{
    idt_install(0x0E, page_fault_handler, true);

    /*
     * Create initial pagedir: just identity-map kernel memory
     */
    uint32_t* pagedir = vmm_create_pagedir();
    for(unsigned char* page = (unsigned char*)KERNEL_START;
            page < (unsigned char*)kmalloc_brk();
            page += PAGE_SIZE) {

        size_t pde_index = PDE_INDEX(page);
        size_t pte_index = PTE_INDEX(page);

        if(!(pagedir[pde_index] & PDE_PRESENT)) {
            uint32_t* pagetable = vmm_create_pagetable();
            pagedir[pde_index] = ENTRY(PDE_PRESENT|PDE_WRITABLE, pagetable);
        }

        uint32_t* pagetable = (uint32_t*)(pagedir[pde_index] & PDE_FRAME);
        assert(pagetable[pte_index] == 0);

        pagetable[pte_index] = ENTRY(PTE_PRESENT|PTE_WRITABLE, page);
    }

    /* Recursive mapping */
    pagedir[1023] = ENTRY(PDE_PRESENT|PDE_WRITABLE, pagedir);

    /* Enable paging */
    write_cr3((unsigned long)pagedir);
    unsigned long cr3 = read_cr3();
    assert2(cr3 == (unsigned long)pagedir, "pagedir: %p, cr3: %p", pagedir, cr3);

    unsigned long cr0 = read_cr0();
    cr0 |= CR0_PG | CR0_WP;
    write_cr0(cr0);

    is_initialized = true;
}

bool vmm_initialized()
{
    return is_initialized;
}

void vmm_map(void* page, unsigned long frame, unsigned flags)
{
    if(!(get_pde(page) & PDE_PRESENT)) {
        uint32_t* pagetable = vmm_create_pagetable();
        uint32_t pde = ENTRY(PDE_PRESENT|PDE_WRITABLE, vmm_get_frame(pagetable));
        set_pde(page, pde);
    }

    assert2(!(get_pte(page) & PTE_PRESENT), "Page %p already mapped to %p", page, vmm_get_frame(page));

    unsigned pte_flags = PTE_PRESENT;
    if(flags & VMM_PAGE_WRITABLE)
        pte_flags |= PTE_WRITABLE;
    if(flags & VMM_PAGE_USER)
        pte_flags |= PTE_USER;

    set_pte(page, ENTRY(pte_flags, frame));
    invlpg(page);

    //trace("Mapped %p to %p", page, frame);
}

void vmm_unmap(void* page)
{
    assert2(get_pde(page) & PDE_PRESENT, "Page %p already unmapped", page);
    assert2(get_pte(page) & PTE_PRESENT, "Page %p already unmapped", page);

    set_pte(page, get_pte(page) & ~PTE_PRESENT);
    invlpg(page);
}

void vmm_remap(void* page, unsigned flags)
{
    assert2(get_pde(page) & PDE_PRESENT, "Page %p not mapped", page);

    unsigned long pte = get_pte(page);
    assert2(pte & PTE_PRESENT, "Page %p not mapped", page);

    unsigned pte_flags = PTE_PRESENT;
    if(flags & VMM_PAGE_WRITABLE)
        pte_flags |= PTE_WRITABLE;
    if(flags & VMM_PAGE_USER)
        pte_flags |= PTE_USER;
    set_pte(page, pte | pte_flags);
    invlpg(page);
}

uint32_t vmm_get_frame(void* page)
{
    assert2(get_pde(page) & PDE_PRESENT, "Page %p not mapped", page);
    assert2(get_pte(page) & PTE_PRESENT, "Page %p not mapped", page);
    return get_pte(page) & PTE_FRAME;
}

uint32_t* vmm_create_pagedir()
{
    uint32_t* pagedir = kmalloc_aligned(sizeof(uint32_t) * 1024, PAGE_SIZE);
    assert((((unsigned long)pagedir) % PAGE_SIZE) == 0);
    bzero(pagedir, sizeof(uint32_t) * 1024);
    return pagedir;
}

uint32_t* vmm_create_pagetable()
{
    uint32_t* pagetable = kmalloc_aligned(sizeof(uint32_t) * 1024, PAGE_SIZE);
    assert((((unsigned long)pagetable) % PAGE_SIZE) == 0);
    bzero(pagetable, sizeof(uint32_t) * 1024);
    return pagetable;
}

void test_vmm()
{
    trace(" -= Testing vmm =-");

    /*
     * 0x0      - 0x3FFFFF         mapped
     * 0x400000 - 0x7DFFFF         valid
     */
    unsigned long* ptr = (unsigned long*)0x400000;
    vmm_map(ptr, 0x400000, VMM_PAGE_WRITABLE);     /* Should not throw an error */
    for(size_t i = 0; i < 1024; i++) {
        ptr[i] = 0xDEADBEEF;
    }

    //*((unsigned long*)0x401000) = 0xAABBCCDD;                    /* page fault */

    ptr = (unsigned long*)0x401000;
    vmm_map(ptr, 0x400000, VMM_PAGE_WRITABLE);
    for(size_t i = 0; i < 1024; i++) {
        assert2(ptr[i] == 0xDEADBEEF, "ptr[%d]: %p", i, ptr[i]);
    }
    vmm_unmap(ptr);
    //*((unsigned long*)ptr) = 0xAABBCCDD;   /* page fault */

    vmm_map(ptr, 0x400000, 0);
    //*ptr = 'a';    /* page fault */
    vmm_remap(ptr, VMM_PAGE_WRITABLE);
    *ptr = 'A';

    /* Alloc a big block to test kmalloc integration */
    ptr = kmalloc(PAGE_SIZE * 16);
    bzero(ptr, PAGE_SIZE * 16);
    kfree(ptr);

    /* Reset */


    trace(" -= Done testing vmm =-");
}


