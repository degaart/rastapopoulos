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

#define PAGE_SIZE       4096
#define RECURSIVE_INDEX 1023
#define CR0_PG          0x80000000
#define KERNEL_OFFSET   0xc0000000

static uint32_t __attribute__((aligned(PAGE_SIZE))) pagedir[1024];
static uint32_t memmap_storage[(BOOTSTRAP_END / PAGE_SIZE / 32)];
static Bitset memmap;
extern uint8_t __kernel_phys_end[];
extern void higher_half_start(void);

static void* alloc_page(void)
{
    size_t frame_idx = bitset_find(&memmap);
    if (frame_idx == SIZE_MAX)
        return NULL;
    bitset_set(&memmap, frame_idx);
    return (void*)(frame_idx * PAGE_SIZE);
}

/*
 * pde index: address >> 22
 * pte index: (address >> 12) & 0x3ff
 */
static void map(uintptr_t va, uintptr_t pa, unsigned flags)
{
    size_t pde_idx = va >> 22;
    if ((pagedir[pde_idx] & PDE_PRESENT) == 0) {
        uint32_t* pagetable = alloc_page();
        if (!pagetable)
            panic("OOM while mapping 0x%x", va);
        memset(pagetable, 0, sizeof(uint32_t) * 1024);
        pagedir[pde_idx] = (uintptr_t)pagetable | PDE_PRESENT | PDE_WRITABLE;
    }

    uint32_t* pagetable = (uint32_t*)(pagedir[pde_idx] & PDE_ADDRESS_MASK);
    size_t pte_idx = (va >> 12) & 0x3ff;
    if ((pagetable[pte_idx] & PTE_PRESENT) != 0)
        panic("Page 0x%x already mapped to 0x%x", va,
              pagetable[pte_idx] & PTE_ADDRESS_MASK);
    pagetable[pte_idx] = (pa & PTE_ADDRESS_MASK) | PTE_PRESENT | flags;
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

    struct Gdtr gdtr = {/* Skip the last 3 entries of struct Gdt (usercode,
                           userdata, and tss) */
                        .limit =
                            sizeof(gdt) - (sizeof(struct GdtEntry) * 3) - 1,
                        .base = (uintptr_t)&gdt};
    asm volatile("lgdt [%0]\n\t"
                 "jmp 0x8:reload_cs\n\t"
                 "reload_cs:"
                 :
                 : "r"(&gdtr)
                 : "memory");
    /*
     * setup initial pagedir
     * Maps:
     *       __bootstrap_start       - __bootstrap_start+4Mb
     *       __kernel_start          - __kernel_end
     * We map a fixed 4Mb at the bootstrap area because we don't know
     * how much arena we need beforehand. Hope this works
     *
     * TODO: We can reduce memory consumption by not including low memory
     * in memmap
     */
    /*
     * Initialize the bootstrap memmap for use when we allocate PDE and PTEs
     * Notice we don't touch memory < 1Mb or > 4Mb
     */
    assert((uintptr_t)__kernel_phys_end < BOOTSTRAP_END);
    static const size_t memmap_size = BOOTSTRAP_END / PAGE_SIZE;
    assert(bitset_storage_size(memmap_size) == sizeof(memmap_storage));
    bitset_init(&memmap, memmap_storage, memmap_size);
    if (!bitset_set_range(&memmap, 0, memmap_size))
        panic("Failed to initialize bootstrap memmap");
    if (mbinfo->flags & MULTIBOOT_INFO_MEM_MAP) {
        const struct multiboot_mmap_entry* entry =
            (struct multiboot_mmap_entry*)mbinfo->mmap_addr;
        for (; entry < (struct multiboot_mmap_entry*)(mbinfo->mmap_addr +
                                                      mbinfo->mmap_length);
             entry =
                 (struct multiboot_mmap_entry*)((void*)entry + entry->size)) {
            if (entry->type == MULTIBOOT_MEMORY_AVAILABLE &&
                entry->addr < UINT32_MAX) {
                uint32_t e_addr = entry->addr;
                uint32_t e_len = entry->len;
                for (uintptr_t frame = (uintptr_t)__kernel_phys_end;
                     frame < e_addr + e_len && frame < BOOTSTRAP_END;
                     frame += PAGE_SIZE) {
                    if (!bitset_unset(&memmap, frame / PAGE_SIZE))
                        panic("Failed to mark frame 0x%x as available", frame);
                }
            }
        }
    } else if (mbinfo->flags & MULTIBOOT_INFO_MEMORY) {
        size_t mem_limit = (mbinfo->mem_upper * 1024) + BOOTSTRAP_START;
        trace("mem_limit: 0x%x", mem_limit);
        for (uintptr_t frame = (uintptr_t)__kernel_phys_end;
             frame < mem_limit && frame < BOOTSTRAP_END; frame += PAGE_SIZE) {
            if (!bitset_unset(&memmap, frame / PAGE_SIZE))
                panic("Failed to mark frame 0x%x as available", frame);
        }
    } else {
        panic("Not memory info. Cannot continue");
    }
    trace("Available memory for bootstrap: %u bytes",
          bitset_count_unset(&memmap) * PAGE_SIZE);
    trace("__kernel_phys_end: 0x%x", __kernel_phys_end);

    size_t next_frame = bitset_find(&memmap);

    /* Recursive PDE */
    pagedir[RECURSIVE_INDEX] = (uintptr_t)pagedir | PDE_PRESENT | PDE_WRITABLE;

    /*
     * Identity-map the first 4Mb
     * TODO: Also check multiboot info for end of high memory
     * (for 2MB machines)
     */
    for (uintptr_t ptr = 0; ptr < BOOTSTRAP_END; ptr += PAGE_SIZE) {
        map(ptr, ptr, PTE_PRESENT | PTE_WRITABLE);
    }

    /*
     * Map the higher-half of address-space to the lower-half
     */
    for (uintptr_t ptr = 0; ptr < BOOTSTRAP_END; ptr += PAGE_SIZE) {
        map(ptr + KERNEL_OFFSET, ptr, PTE_PRESENT | PTE_WRITABLE);
    }

    /* Enable paging */
    write_cr3((uintptr_t)pagedir);
    write_cr0(read_cr0() | CR0_PG);

    /* Jump to higher-half kernel */
    trace("higher_half_start: 0x%x", (uintptr_t)higher_half_start);
    assert((uintptr_t)higher_half_start > KERNEL_OFFSET);

    uint32_t eax = mbsgn;
    uint32_t ebx = (uintptr_t)mbinfo + KERNEL_OFFSET;
    uint32_t ecx = (uintptr_t)&memmap + KERNEL_OFFSET;
    asm volatile("jmp higher_half_start"
                 :
                 : "a"(eax), "b"(ebx), "c"(ecx)
                 : "memory");
    __builtin_unreachable();
}

