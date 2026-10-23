#include "address_space.h"
#include "assert.h"
#include "early_malloc.h"
#include "kernel.h"
#include "kmalloc.h"
#include "pmm.h"
#include <string.h>

#define PDE_PRESENT       (1 << 0)
#define PDE_WRITABLE      (1 << 1)
#define PDE_USER          (1 << 2)
#define PDE_WRITE_THROUGH (1 << 3)
#define PDE_CACHE_DISABLE (1 << 4)
#define PDE_ACCESSED      (1 << 5)
#define PDE_PAGE_SIZE     (1 << 7)
#define PDE_ADDRESS_MASK  0xfffff000
#define PDE_INDEX(VA)     ((VA) >> 22)

#define PTE_PRESENT       (1 << 0)
#define PTE_WRITABLE      (1 << 1)
#define PTE_USER          (1 << 2)
#define PTE_WRITE_THROUGH (1 << 3)
#define PTE_CACHE_DISABLE (1 << 4)
#define PTE_ACCESSED      (1 << 5)
#define PTE_DIRTY         (1 << 6)
#define PTE_PAT           (1 << 7)
#define PTE_GLOBAL        (1 << 8)
#define PTE_SHARED        (1 << 9)
#define PTE_ADDRESS_MASK  0xfffff000
#define PTE_INDEX(VA)     (((VA) >> 12) & 0x3ff)

#define RECURSIVE_INDEX 1023
#define RECURSIVE_BASE  0xffc00000
#define RECURSIVE_ADDR  0xfffff000

static struct AddressSpaceList address_spaces =
    TAILQ_HEAD_INITIALIZER(address_spaces);
static __attribute__((aligned(PAGE_SIZE))) uint32_t temp_map[1024];
static bool temp_map_flag;
static __attribute__((aligned(PAGE_SIZE))) uint32_t initial_pagedir[1024];
static struct AddressSpace initial_address_space;
static struct AddressSpace* current_address_space;

static bool valid_vaddr(uint32_t vaddr)
{
    return vaddr > 0 && IS_ALIGNED(vaddr, PAGE_SIZE) && vaddr < RECURSIVE_BASE;
}

static bool valid_paddr(uint32_t paddr)
{
    return IS_ALIGNED(paddr, PAGE_SIZE);
}

static bool valid_flags(unsigned flags)
{
    return (flags & PTE_ADDRESS_MASK) == 0;
}

static void flush_tlb(void)
{
    write_cr3(read_cr3());
}

uint32_t* recursive_pagedir()
{
    return (uint32_t*)RECURSIVE_ADDR;
}

uint32_t* recursive_pagetable(size_t pde_idx)
{
    return (uint32_t*)(RECURSIVE_BASE + pde_idx * PAGE_SIZE);
}

NODISCARD static bool recursive_frame(const void* va, uint32_t* phys)
{
    uintptr_t addr = (uintptr_t)va;
    if (!phys || !valid_vaddr(addr))
        return false;

    bool iflag = enter_critical_section();
    size_t pde_idx = PDE_INDEX(addr);
    uint32_t* pagedir = recursive_pagedir();
    if (!(pagedir[pde_idx] & PDE_PRESENT)) {
        leave_critical_section(iflag);
        return false;
    }

    size_t pte_idx = PTE_INDEX(addr);
    uint32_t* pagetable = recursive_pagetable(pde_idx);
    if (!(pagetable[pte_idx] & PTE_PRESENT)) {
        leave_critical_section(iflag);
        return false;
    }

    *phys = (uint32_t)(pagetable[pte_idx] & PTE_ADDRESS_MASK);
    leave_critical_section(iflag);
    return true;
}

#if 0
NODISCARD static bool recursive_remap(void* virtual_address, uint32_t flags)
{
    uintptr_t addr = (uintptr_t)virtual_address;
    if (!valid_vaddr(addr) || !valid_flags(flags))
        return false;

    bool iflag = enter_critical_section();
    size_t pde_idx = PDE_INDEX(addr);
    uint32_t* pagedir = recursive_pagedir();
    if (!(pagedir[pde_idx] & PDE_PRESENT)) {
        leave_critical_section(iflag);
        return false;
    }

    size_t pte_idx = PTE_INDEX(addr);
    uint32_t* pagetable = recursive_pagetable(pde_idx);
    if (!(pagetable[pte_idx] & PTE_PRESENT)) {
        leave_critical_section(iflag);
        return false;
    }

    pagetable[pte_idx] = (pagetable[pte_idx] & PTE_ADDRESS_MASK) | flags;
    flush_tlb();
    leave_critical_section(iflag);
    return true;
}
#endif

NODISCARD static bool recursive_unmap(void* virtual_address)
{
    uintptr_t addr = (uintptr_t)virtual_address;
    if (!valid_vaddr(addr))
        return false;

    bool iflag = enter_critical_section();
    size_t pde_idx = PDE_INDEX(addr);
    uint32_t* pagedir = recursive_pagedir();
    if (!(pagedir[pde_idx] & PDE_PRESENT)) {
        leave_critical_section(iflag);
        return false;
    }

    size_t pte_idx = PTE_INDEX(addr);
    uint32_t* pagetable = recursive_pagetable(pde_idx);
    if (!(pagetable[pte_idx] & PTE_PRESENT)) {
        leave_critical_section(iflag);
        return false;
    }

    pagetable[pte_idx] = 0;

    bool empty = true;
    for (uint32_t i = 0; i < 1024; i++) {
        if (pagetable[i] & PTE_PRESENT) {
            empty = false;
            break;
        }
    }

    if (empty) {
        uint32_t pagetable_phys = pagedir[pde_idx] & PDE_ADDRESS_MASK;
        pagedir[pde_idx] = 0;
        pmm_free((void*)pagetable_phys);
    }

    flush_tlb();
    leave_critical_section(iflag);
    return true;
}

NODISCARD static bool recursive_map(void* frame, void* virtual_address,
                                    uint32_t flags)
{
    uintptr_t vaddr = (uintptr_t)virtual_address;
    uintptr_t paddr = (uintptr_t)frame;
    if (!valid_vaddr(vaddr) || !valid_paddr(paddr) || !valid_flags(flags))
        return false;

    bool iflag = enter_critical_section();
    size_t pde_idx = PDE_INDEX(vaddr);
    size_t pte_idx = PTE_INDEX(vaddr);
    uint32_t* pagedir = recursive_pagedir();
    uint32_t* pagetable = recursive_pagetable(pde_idx);

    if ((pagedir[pde_idx] & PDE_PRESENT) &&
        (pagetable[pte_idx] & PTE_PRESENT)) {
        leave_critical_section(iflag);
        return false;
    }

    if (!(pagedir[pde_idx] & PDE_PRESENT)) {
        uintptr_t pagetable_phys = (uintptr_t)pmm_alloc();
        if (!pagetable_phys) {
            leave_critical_section(iflag);
            return false;
        }

        unsigned flags = PDE_PRESENT | PDE_WRITABLE;
        if (vaddr < KERNEL_OFFSET)
            flags |= PDE_USER;
        pagedir[pde_idx] = pagetable_phys | flags;
        flush_tlb();

        memset(pagetable, 0, 1024 * sizeof(uint32_t));
    }

    pagetable[pte_idx] = paddr | PTE_PRESENT | flags;
    flush_tlb();
    leave_critical_section(iflag);
    return true;
}

static uint32_t* lock_pagetable(uint32_t pagetable)
{
    assert(valid_paddr(pagetable));

    temp_map_flag = enter_critical_section();

    uintptr_t vaddr = (uintptr_t)temp_map;
    uint32_t* pagedir = recursive_pagedir();
    size_t pde_idx = PDE_INDEX(vaddr);
    if (!(pagedir[pde_idx] & PDE_PRESENT))
        panic("Failed to lock pagetable");

    uint32_t* pt = recursive_pagetable(pde_idx);
    pt[PTE_INDEX(vaddr)] = pagetable | PTE_PRESENT | PTE_WRITABLE;
    flush_tlb();
    return temp_map;
}

static void unlock_pagetable(uint32_t* pagetable)
{
    assert(valid_vaddr((uintptr_t)pagetable));

    uint32_t frame;
    if (!recursive_frame(pagetable, &frame))
        goto error;

    uint32_t current_mapped;
    if (!recursive_frame(temp_map, &current_mapped))
        goto error;

    if (frame != current_mapped)
        goto error;

    leave_critical_section(temp_map_flag);
    return;
error:
    panic("Error unlocking pagetable 0x%p", pagetable);
}

/* Assumes current pagedir has a recursive mapping */
void address_space_init(void)
{
    if ((read_cr0() & (CR0_PE | CR0_PG)) != (CR0_PE | CR0_PG))
        goto error;

    /*
     * temp_map is permanent: we just remap it to another address when needed
     * This way, remapping it never needs a PDE allocation, which avoids a
     * circular dependency
     * At initialization, we reclaim its physical page frame
     */
    uintptr_t temp_map_phys = ((uintptr_t)temp_map) - KERNEL_OFFSET;
    if (!recursive_unmap(temp_map))
        goto error;
    if (!recursive_map(0, temp_map, 0))
        goto error;
    pmm_free((void*)temp_map_phys);

    uint32_t initial_pagedir_phys = (uintptr_t)initial_pagedir - KERNEL_OFFSET;
    initial_address_space.pagedir = initial_pagedir;
    initial_address_space.pagedir_phys = initial_pagedir_phys;
    initial_address_space.destroyable = false;
    TAILQ_INSERT_TAIL(&address_spaces, &initial_address_space, node);
    current_address_space = &initial_address_space;

    uintptr_t start = (uintptr_t)__kernel_start;
    uintptr_t end = (uintptr_t)early_malloc_tail();
    if (start >= end || end > RECURSIVE_BASE)
        goto error;

    uint32_t* pagedir = initial_pagedir;
    for (uintptr_t addr = start & PTE_ADDRESS_MASK; addr < end;
         addr += PAGE_SIZE) {
        size_t pde_idx = PDE_INDEX(addr);
        size_t pte_idx = PTE_INDEX(addr);
        if (!(pagedir[pde_idx] & PDE_PRESENT)) {
            uint32_t pagetable_phys = (uint32_t)pmm_alloc();
            if (!pagetable_phys)
                goto error;

            /* We're still identity-mapped, so mister pagetable_phys here is
             * available at KERNEL_OFFSET */
            uint32_t* pagetable = (uint32_t*)(pagetable_phys + KERNEL_OFFSET);
            memset(pagetable, 0, 1024 * sizeof(uint32_t));
            pagedir[pde_idx] = pagetable_phys | PDE_PRESENT | PDE_WRITABLE;
        }

        uint32_t pagetable_phys = pagedir[pde_idx] & PDE_ADDRESS_MASK;
        uint32_t* pagetable = (uint32_t*)(pagetable_phys + KERNEL_OFFSET);

        if (addr == (uintptr_t)&temp_map) {
            pagetable[pte_idx] = 0 | PTE_PRESENT;
        } else {
            unsigned flags = PTE_WRITABLE;
            if (addr >= (uintptr_t)__kernel_text_start &&
                addr + PAGE_SIZE < (uintptr_t)__kernel_text_end)
                flags &= ~PTE_WRITABLE;
            if (addr >= (uintptr_t)__kernel_rodata_start &&
                addr + PAGE_SIZE < (uintptr_t)__kernel_rodata_end)
                flags &= ~PTE_WRITABLE;
            pagetable[pte_idx] = (addr - KERNEL_OFFSET) | PTE_PRESENT | flags;
        }
    }
    pagedir[RECURSIVE_INDEX] =
        initial_pagedir_phys | PTE_PRESENT | PTE_WRITABLE;
    write_cr3(initial_pagedir_phys);
    write_cr0(read_cr0() | CR0_WP);
    early_malloc_disable();
    return;
error:
    panic("%s failed", __FUNCTION__);
}

bool address_space_create(struct AddressSpace* as)
{
    memset(as, 0, sizeof(*as));
    as->pagedir = kmemalign(PAGE_SIZE, 1024 * sizeof(uint32_t));
    if (!as->pagedir)
        return false;

    memset(as->pagedir, 0, 1024 * sizeof(uint32_t));
    if (!recursive_frame(as->pagedir, &as->pagedir_phys))
        panic("Failed to get frame address of 0x%p", as->pagedir);
    as->destroyable = true;

    as->pagedir[RECURSIVE_INDEX] =
        as->pagedir_phys | PDE_PRESENT | PDE_WRITABLE;

    /* Copy current kernel mappings */
    bool iflag = enter_critical_section();
    uint32_t* current_pagedir = (uint32_t*)RECURSIVE_ADDR;
    for (int i = PDE_INDEX(KERNEL_OFFSET); i < RECURSIVE_INDEX; i++) {
        as->pagedir[i] = current_pagedir[i];
    }
    TAILQ_INSERT_TAIL(&address_spaces, as, node);
    leave_critical_section(iflag);

    return true;
}

void address_space_destroy(struct AddressSpace* as)
{
    if (!as->destroyable)
        panic("Attempt to destroy non-destroyable address space 0x%p", as);
    if (read_cr3() == as->pagedir_phys)
        panic("Attempt to destroy current address space");

    bool iflag = enter_critical_section();
    TAILQ_REMOVE(&address_spaces, as, node);
    leave_critical_section(iflag);

    /* Free user-space pagetables */
    for (int i = 0; i < PDE_INDEX(KERNEL_OFFSET); i++) {
        if (as->pagedir[i] & PDE_PRESENT) {
            uint32_t* pagetable =
                lock_pagetable(as->pagedir[i] & PDE_ADDRESS_MASK);
            for (int j = 0; j < 1024; j++) {
                if ((pagetable[j] & PTE_PRESENT) &&
                    !(pagetable[j] & PTE_SHARED)) {
                    pmm_free((void*)(pagetable[j] & PTE_ADDRESS_MASK));
                }
            }
            pmm_free((void*)(as->pagedir[i] & PDE_ADDRESS_MASK));
            unlock_pagetable(pagetable);
        }
    }

    kfree(as->pagedir);
}

void address_space_activate(struct AddressSpace* as)
{
    bool iflag = enter_critical_section();
    current_address_space = as;
    write_cr3(as->pagedir_phys);
    leave_critical_section(iflag);
}

struct AddressSpace* address_space_current(void)
{
    return current_address_space;
}

bool address_space_map(struct AddressSpace* as, void* pa, void* va,
                       unsigned flags)
{
    uintptr_t vaddr = (uintptr_t)va;
    uintptr_t paddr = (uintptr_t)pa;

    if (!valid_vaddr(vaddr) || !valid_paddr(paddr) || !valid_flags(flags))
        return false;

    volatile uint32_t* pagedir = as->pagedir;

    bool iflag = enter_critical_section();
    size_t pde_idx = PDE_INDEX(vaddr);
    uint32_t pagetable_phys = (uintptr_t)pagedir[pde_idx] & PDE_ADDRESS_MASK;
    if ((pagedir[pde_idx] & PDE_PRESENT) == 0) {
        pagetable_phys = (uint32_t)pmm_alloc();
        if (!pagetable_phys) {
            leave_critical_section(iflag);
            return false;
        }

        /*
         * Recursive mapping only works for the current pagedir, so we have to
         * use temporary mappings Notice IF is disabled betweek lock/unlock
         * pairs
         */
        uint32_t* pageframe = lock_pagetable(pagetable_phys);
        memset(pageframe, 0, 1024 * sizeof(uint32_t));
        unlock_pagetable(pageframe);

        unsigned pde_flags = PDE_PRESENT | PDE_WRITABLE;
        if (vaddr < KERNEL_OFFSET)
            pde_flags |= PDE_USER;
        pagedir[pde_idx] = pagetable_phys | pde_flags;

        /* Update all other address spaces if vaddr is in kernel space */
        if (vaddr >= KERNEL_OFFSET) {
            struct AddressSpace* a;
            TAILQ_FOREACH(a, &address_spaces, node)
            {
                if (a != as)
                    a->pagedir[pde_idx] = pagedir[pde_idx];
            }
            flush_tlb();
        }
    }

    size_t pte_idx = PTE_INDEX(vaddr);
    uint32_t* pageframe = lock_pagetable(pagetable_phys);
    if (pageframe[pte_idx] & PTE_PRESENT) {
        unlock_pagetable(pageframe);
        leave_critical_section(iflag);
        return false;
    }
    pageframe[pte_idx] = paddr | PDE_PRESENT | flags;
    unlock_pagetable(pageframe);
    leave_critical_section(iflag);
    return true;
}

bool address_space_unmap(struct AddressSpace* as, void* va)
{
    uint32_t vaddr = (uintptr_t)va;
    if (!valid_vaddr(vaddr))
        return false;

    size_t pde_idx = PDE_INDEX(vaddr);
    bool iflag = enter_critical_section();
    volatile uint32_t* pagedir = as->pagedir;
    if ((pagedir[pde_idx] & PDE_PRESENT) == 0) {
        leave_critical_section(iflag);
        return false;
    }

    size_t pte_idx = PTE_INDEX(vaddr);
    uint32_t pagetable_phys = pagedir[pde_idx] & PDE_ADDRESS_MASK;
    uint32_t* pagetable = lock_pagetable(pagetable_phys);

    uint32_t pte = pagetable[pte_idx];
    if ((pte & PTE_PRESENT) == 0) {
        unlock_pagetable(pagetable);
        leave_critical_section(iflag);
        return false;
    }

    if (!(pte & PTE_SHARED)) {
        pmm_free((void*)(pte & PTE_ADDRESS_MASK));
    }
    pagetable[pte_idx] = 0;

    /* If all pages in this pagetable are unmapped, remove it from the pagedir
     */
    bool empty = true;
    for (int i = 0; i < 1024; i++) {
        if (pagetable[i] & PTE_PRESENT) {
            empty = false;
            break;
        }
    }

    if (empty) {
        as->pagedir[pde_idx] = 0;

        /* If in kernel space, remove from all other pagedirs */
        if (vaddr >= KERNEL_OFFSET) {
            struct AddressSpace* a;
            TAILQ_FOREACH(a, &address_spaces, node)
            {
                if (a != as)
                    a->pagedir[pde_idx] = 0;
            }
        }
    }

    flush_tlb();
    unlock_pagetable(pagetable);

    if (empty) {
        pmm_free((void*)pagetable_phys);
    }

    leave_critical_section(iflag);
    return true;
}

bool address_space_remap(struct AddressSpace* as, void* va, unsigned flags)
{
    uint32_t vaddr = (uint32_t)va;
    if (!valid_vaddr(vaddr) || !valid_flags(flags))
        return false;

    bool iflag = enter_critical_section();
    size_t pde_idx = PDE_INDEX(vaddr);
    if ((as->pagedir[pde_idx] & PDE_PRESENT) == 0) {
        leave_critical_section(iflag);
        return false;
    }

    uint32_t pagetable_phys = as->pagedir[pde_idx] & PDE_ADDRESS_MASK;
    uint32_t* pagetable = lock_pagetable(pagetable_phys);
    size_t pte_idx = PTE_INDEX(vaddr);
    if ((pagetable[pte_idx] & PTE_PRESENT) == 0) {
        unlock_pagetable(pagetable);
        leave_critical_section(iflag);
        return false;
    }

    pagetable[pte_idx] =
        (pagetable[pte_idx] & PTE_ADDRESS_MASK) | PTE_PRESENT | flags;
    flush_tlb();

    unlock_pagetable(pagetable);
    leave_critical_section(iflag);
    return true;
}

static bool user_page_accessible(uintptr_t addr, bool for_write)
{
    uint32_t pde = recursive_pagedir()[PDE_INDEX(addr)];
    unsigned pde_flags = PDE_PRESENT | PDE_USER;
    unsigned pte_flags = PTE_PRESENT | PTE_USER;
    if (for_write) {
        pde_flags |= PDE_WRITABLE;
        pte_flags |= PTE_WRITABLE;
    }
    if ((pde & pde_flags) != pde_flags)
        return false;
    if (pde & PDE_PAGE_SIZE)
        return true;

    uint32_t pte = recursive_pagetable(PDE_INDEX(addr))[PTE_INDEX(addr)];
    return (pte & pte_flags) == pte_flags;
}

bool address_space_is_valid_userptr(const void* ptr, size_t len,
                                    bool for_write)
{
    uintptr_t addr = (uintptr_t)ptr;
    if (!addr || addr >= KERNEL_OFFSET || len > KERNEL_OFFSET - addr)
        return false;
    if (!len)
        return true;

    uintptr_t end = addr + len;
    bool iflag = enter_critical_section();
    for (uintptr_t page = addr & PTE_ADDRESS_MASK; page < end;
         page += PAGE_SIZE) {
        if (!user_page_accessible(page, for_write)) {
            leave_critical_section(iflag);
            return false;
        }
    }
    leave_critical_section(iflag);
    return true;
}

bool address_space_is_valid_userstr(const char* str)
{
    uintptr_t addr = (uintptr_t)str;
    if (!addr || addr >= KERNEL_OFFSET)
        return false;

    bool iflag = enter_critical_section();
    while (addr < KERNEL_OFFSET) {
        if (!user_page_accessible(addr, false)) {
            leave_critical_section(iflag);
            return false;
        }
        uintptr_t end = (addr & PTE_ADDRESS_MASK) + PAGE_SIZE;
        if (memchr((const void*)addr, '\0', end - addr)) {
            leave_critical_section(iflag);
            return true;
        }
        addr = end;
    }
    leave_critical_section(iflag);
    return false;
}

/* TODO: This can be optimized by using the recursive pagedir if as is the
 * current address space */
bool address_space_query(struct AddressSpace* as, const void* va, void** frame,
                         unsigned* flags)
{
    uint32_t vaddr = (uintptr_t)va;
    if (!valid_vaddr(vaddr))
        return false;

    bool iflag = enter_critical_section();
    size_t pde_idx = PDE_INDEX(vaddr);
    if ((as->pagedir[pde_idx] & PDE_PRESENT) == 0) {
        leave_critical_section(iflag);
        return false;
    }

    size_t pte_idx = PTE_INDEX(vaddr);
    uint32_t pagetable_phys = as->pagedir[pde_idx] & PDE_ADDRESS_MASK;
    uint32_t* pagetable = lock_pagetable(pagetable_phys);
    if ((pagetable[pte_idx] & PTE_PRESENT) == 0) {
        unlock_pagetable(pagetable);
        leave_critical_section(iflag);
        return false;
    }

    if (frame)
        *frame = (void*)(pagetable[pte_idx] & PTE_ADDRESS_MASK);
    if (flags)
        *flags = pagetable[pte_idx] & ~PTE_ADDRESS_MASK;
    unlock_pagetable(pagetable);
    leave_critical_section(iflag);
    return true;
}

