#include "../debug.h"
#include "../pmm.h"
#include "../vmm.h"
#include "../registers.h"
#include "../kernel.h"
#include "../util.h"
#include "../idt.h"

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

static bool is_identity_mapped(uint32_t address)
{
    uint32_t* current_pagedir = (uint32_t*)read_cr3();

    uint32_t pde_index = PAGE_DIRECTORY_INDEX(address);
    uint32_t pte_index = PAGE_TABLE_INDEX(address);

    assert(current_pagedir[pde_index] & PDE_PRESENT);
    uint32_t* table = (uint32_t*) (current_pagedir[pde_index] & PDE_FRAME);

    assert(table[pte_index] & PTE_PRESENT);
    uint32_t va = table[pte_index] & PTE_FRAME;

    bool result = (va == TRUNCATE(address, PAGE_SIZE));
    return result;
}

static void test_identity_mapped()
{
    trace("Testing kernel is properly identity-mapped");

    /* Test kernel is properly identity-paged */
    for(uint32_t page = KERNEL_START; page < KERNEL_END; page += PAGE_SIZE) {
        assert(is_identity_mapped(page));
    }
}

static void test_recursive_mapping()
{
    trace("Testing recursive mapping");

    /* Last PDE should point to cr3 */
    uint32_t* cr3 = (uint32_t*)read_cr3();
    uint32_t* last_pde = (uint32_t*)(cr3[1023] & PDE_FRAME);
    assert(last_pde == cr3);

    /* Which means we current pagedir is mapped at 0xFFFFF000 */
    uint32_t* pagedir = (uint32_t*)0xFFFFF000;
    for(int i = 0; i < 1024; i++)
        assert(pagedir[i] == cr3[i]);

    /* And we can access a particular page table at (0xFFC00000 + (pte * PAGE_SIZE)) */
    uint32_t pde_index = PAGE_DIRECTORY_INDEX(KERNEL_START);
    uint32_t pte_index = PAGE_TABLE_INDEX(KERNEL_START);

    uint32_t* phys_pagetable = (uint32_t*)(cr3[pde_index] & PDE_FRAME);
    uint32_t* va_pagetable = (uint32_t*)(0xFFC00000 + (pde_index * PAGE_SIZE));

    for(int i = 0; i < 1024; i++) {
        assert(phys_pagetable[i] == va_pagetable[i]);
    }
}

static void test_map()
{
    trace("Testing vmm_map");

    /* Try mapping a new page at 32Mb */
    volatile uint32_t* my_page = (uint32_t*)(32 * 1024 * 1024);
    uint32_t page_frame = pmm_alloc();
    vmm_map((uint32_t)my_page, page_frame, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);

    /* read and write from it */
    uint32_t value = *my_page;
    value ^= 7;
    *my_page = value;

    /* Unmap it */
    vmm_unmap((uint32_t)my_page);
    pmm_free(page_frame);
}

static void* unmap_env[20];

static void unmap_pf_handler(struct isr_regs* regs)
{
    __builtin_longjmp(unmap_env, 1);
    assert(!"longjmp failed");
}

static void test_unmap()
{
    trace("Testing vmm_unmap");

    /* Map a new page at 33Mb */
    volatile uint32_t* test_page = (uint32_t*)(33 * 1024 * 1024);
    uint32_t page_frame = pmm_alloc();
    vmm_map((uint32_t)test_page, page_frame, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);

    /* Test we can read and write from that page */
    uint32_t val = *test_page;
    val ^= 7;
    *test_page = val;

    /* Unmap that page */
    vmm_unmap((uint32_t)test_page);
    pmm_free(page_frame);

    /* Install page fault handler so we can longjmp back */
    isr_handler_t old_handler = idt_get_handler(14);
    idt_install(14, unmap_pf_handler, false);
    int status = __builtin_setjmp(unmap_env);
    if(status) {
        idt_install(14, old_handler, false);
        return;
    }

    val = *test_page;
    val ^= 0xB00B5;
    *test_page = val;

    assert(!"No page fault!");
}

void test_vmm()
{
    trace("Testing vmm");

    /* Test kernel properly identity-mapped */
    test_identity_mapped();

    /* Test last pagedir entry is recursively mapped to itself */
    test_recursive_mapping();

    /* Test vmm_map */
    test_map();

    /* Test vmm_unmap */
    test_unmap();
}

