/*
 * This file runs in the bootstrap context, which means, only low
 * addresses are available
 * It needs the linker magic in kernel.ld to place all its data/code
 * into .bootstrap.* sections
 */
#include "bitset.h"
#include "bootstrap_util.h"
#include "multiboot.h"

#define BOOTSTRAP_START 0x100000
#define BOOTSTRAP_END   0x400000

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
#define PTE_ADDRESS_MASK  0xfffff000
#define PTE_INDEX(VA)     (((VA) >> 12) & 0x3ff)

#define PAGE_SIZE       4096
#define RECURSIVE_INDEX 1023
#define CR0_PG          0x80000000
#define KERNEL_OFFSET   0xc0000000
#define LOWHALF_LIMIT   0x400000

#define IS_ALIGNED(value, alignment)                                          \
    (((uintptr_t)(value) & ((uintptr_t)(alignment) - 1)) == 0)

static uint32_t __attribute__((aligned(PAGE_SIZE))) pagedir[1024];
static uint32_t __attribute__((aligned(PAGE_SIZE))) pagetable[1024];

extern uint8_t __kernel_phys_start[];
extern uint8_t __kernel_phys_end[];
extern uint8_t __kernel_start[];
extern uint8_t __kernel_end[];
extern void higher_half_start(void);

static void set_pagetable(void* va, uint32_t* pagetable)
{
    uintptr_t addr = (uintptr_t)va;
    assert(IS_ALIGNED(addr, PAGE_SIZE));

    size_t idx = PDE_INDEX(addr);
    pagedir[idx] = (uintptr_t)pagetable | PDE_PRESENT | PDE_WRITABLE;
}

static void map(uintptr_t va, uintptr_t pa, unsigned flags)
{
    assert(IS_ALIGNED(va, PAGE_SIZE));
    assert(IS_ALIGNED(pa, PAGE_SIZE));
    assert(va < LOWHALF_LIMIT);

    size_t idx = PTE_INDEX(va);
    assert((pagetable[idx] & PTE_PRESENT) == 0);
    pagetable[idx] = pa | PTE_PRESENT | flags;
}

static void unmap(uintptr_t va)
{
    assert(IS_ALIGNED(va, PAGE_SIZE));

    size_t idx = PTE_INDEX(va);
    assert((pagetable[idx] & PTE_PRESENT) != 0);
    pagetable[idx] = 0;
}

void bootstrap(uint32_t mbsgn, const struct multiboot_info* mbinfo)
{
    serial_init();
    vga_init();

    if (mbsgn != MULTIBOOT_BOOTLOADER_MAGIC)
        panic("Unsupported bootloader (magic=0x%x)", mbsgn);

    /* Setup initial GDT */
    trace("Bootstrapping kernel");
    struct Gdt gdt = {0};
    bootstrap_gdt_set_entry(&gdt.null, 0, 0, 0, 0);
    bootstrap_gdt_set_entry(&gdt.code, 0, 0xfffff, GDT_ACCESS_CODE,
                            GDT_FLAGS_32BIT_4K);
    bootstrap_gdt_set_entry(&gdt.data, 0, 0xfffff, GDT_ACCESS_DATA,
                            GDT_FLAGS_32BIT_4K);

    struct Gdtr gdtr = {.limit = sizeof(gdt) - 1, .base = (uintptr_t)&gdt};
    asm volatile("lgdt [%0]\n\t"
                 "jmp 0x8:reload_cs\n\t"
                 "reload_cs:"
                 :
                 : "r"(&gdtr)
                 : "memory");

    /*
     * Initial pagedir:
     *  Identity map the first 4Mb of the address space
     *  Then map KERNEL_OFFSET to the first 4Mb
     *  This should give a ~3Mb address space for kernel code, data, and bss
     *  Work memory needed:
     *      - 4k for pagedir
     *      - 4k for shared pagetable lower-half + higher-half
     */
    assert(__kernel_phys_start <
           (uint8_t*)LOWHALF_LIMIT); /* Kernel too large */
    pagedir[RECURSIVE_INDEX] = (uintptr_t)pagedir | PDE_PRESENT | PDE_WRITABLE;
    set_pagetable(0, pagetable);
    set_pagetable((void*)KERNEL_OFFSET, pagetable);

    if (mbinfo->flags & MULTIBOOT_INFO_MEM_MAP) {
        const struct multiboot_mmap_entry* entry =
            (struct multiboot_mmap_entry*)mbinfo->mmap_addr;
        for (; entry < (struct multiboot_mmap_entry*)(mbinfo->mmap_addr +
                                                      mbinfo->mmap_length);
             entry =
                 (struct multiboot_mmap_entry*)((void*)entry + entry->size)) {
            if (entry->type == MULTIBOOT_MEMORY_AVAILABLE &&
                entry->addr <= LOWHALF_LIMIT) {
                uint32_t addr = entry->addr;
                uint32_t len = entry->len;
                if (addr + len > LOWHALF_LIMIT)
                    len = 0x400000 - addr;
                for (uintptr_t page = addr; page < addr + len;
                     page += PAGE_SIZE) {
                    map(page, page, PTE_WRITABLE);
                }
            }
        }
    } else if (mbinfo->flags & MULTIBOOT_INFO_MEMORY) {
        uint32_t lowmem = mbinfo->mem_lower * 1024;
        trace("Map 0x%x - 0x%x", 0, lowmem - 1);
        for (uintptr_t page = 0; page < lowmem; page += PAGE_SIZE) {
            map(page, page, PTE_WRITABLE);
        }

        uint32_t highmem = mbinfo->mem_upper * 1024;
        if (highmem > LOWHALF_LIMIT)
            highmem = LOWHALF_LIMIT - 0x100000;

        trace("highmem: 0x%x", highmem);
        trace("Map 0x%x - 0x%x", 0x100000, 0x100000 + highmem - 1);
        for (uintptr_t page = 0x100000; page < 0x100000 + highmem;
             page += PAGE_SIZE) {
            map(page, page, PTE_WRITABLE);
        }
    } else {
        panic("No memory info. Cannot continue");
    }

    map(VGA_BASE, VGA_BASE, PTE_WRITABLE);
    unmap(0);

    /* Enable paging */
    write_cr3((uintptr_t)pagedir);
    write_cr0(read_cr0() | CR0_PG);

    /* Jump to higher-half kernel */
    trace("higher_half_start: 0x%x", (uintptr_t)higher_half_start);
    assert((uintptr_t)higher_half_start > KERNEL_OFFSET);

    uint32_t eax = mbsgn;
    uint32_t ebx = (uintptr_t)mbinfo + KERNEL_OFFSET;
    asm volatile("jmp higher_half_start" : : "a"(eax), "b"(ebx) : "memory");
    __builtin_unreachable();
}

