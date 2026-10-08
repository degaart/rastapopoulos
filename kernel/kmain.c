#include "early_malloc.h"
#include "idt.h"
#include "kernel.h"
#include "kmalloc.h"
#include "pmm.h"
#include "vga.h"
#include "vmm.h"
#include <assert.h>
#include <gdt.h>
#include <multiboot.h>
#include <stdio.h>
#include <string.h>

#define debugbreak() asm volatile("xchg bx, bx" ::: "memory")

static void dump_multiboot(const struct multiboot_info* info)
{
    printf("Multiboot info:\n");
    if (info->flags & MULTIBOOT_INFO_MEMORY) {
        printf("    mem_lower: 0x%lx, mem_upper: 0x%lx\n", info->mem_lower,
               info->mem_upper);
    }
    if (info->flags & MULTIBOOT_INFO_BOOTDEV) {
        printf("    boot_device: 0x%lx\n", info->boot_device);
    }
    if (info->flags & MULTIBOOT_INFO_MEM_MAP) {
        const struct multiboot_mmap_entry* entry =
            (struct multiboot_mmap_entry*)info->mmap_addr;
        for (; entry < (struct multiboot_mmap_entry*)(info->mmap_addr +
                                                      info->mmap_length);
             entry =
                 (struct multiboot_mmap_entry*)((void*)entry + entry->size)) {
            const char* type = NULL;
            switch (entry->type) {
            case MULTIBOOT_MEMORY_AVAILABLE:
                type = "AVL";
                break;
            case MULTIBOOT_MEMORY_RESERVED:
                type = "RSVD";
                break;
            case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE:
                type = "ACPI";
                break;
            case MULTIBOOT_MEMORY_NVS:
                type = "NVS";
                break;
            case MULTIBOOT_MEMORY_BADRAM:
                type = "BAD";
                break;
            default:
                type = "UNK";
                break;
            }

            printf("    mmap: 0x%08llx-0x%08llx 0x%08llx %s\n", entry->addr,
                   entry->addr + entry->len - 1, entry->len, type);
        }
    }
    if (info->flags & MULTIBOOT_INFO_BOOT_LOADER_NAME) {
        printf("    bootloader: %s\n", (char*)info->boot_loader_name);
    }
}

static void test_heap(void)
{
    kfree(NULL);
    assert(!kmalloc(0));
    assert(!kmemalign(0, 1));
    assert(!kmemalign(3, 1));
    assert(!kmalloc(SIZE_MAX));
    assert(!kmemalign(4096, SIZE_MAX - 16));
    assert(!kmemalign((SIZE_MAX / 2) + 1, 1));

    for (size_t a = 1; a <= (1u << 20); a *= 2) {
        const size_t sizes[] = {1, 3, 15, 16, 17, 4095, 4096, 4097, 65537};
        for (size_t i = 0; i < sizeof(sizes) / sizeof(*sizes); ++i) {
            printf("a=%zu, size=%zu\n", a, sizes[i]);
            if (a == 2 && sizes[i] == 16)
                printf("Here\n");
            void* p = kmemalign(a, sizes[i]);
            memset(p, 0, sizes[i]);
            kfree(p);
        }
    }

#if 0
    fail_next = 1;
    assert(!heap_alloc(100));
    empty();
    void *a = allocate(100, 16);
    size_t before = mapped;
    fail_next = 1;
    assert(!heap_alloc(100000));
    assert(mapped == before);
    for (size_t i = 0; i < 100; ++i) assert(((unsigned char *)a)[i] == 0x5a);
    validate();
    heap_free(a);
    empty();
    /* Coalescing from both sides, then reuse without growth. */
    a = allocate(1000, 16);
    void *b = allocate(1000, 16), *c = allocate(1000, 16);
    void *d = allocate(1000, 16);
    heap_free(a); heap_free(c); heap_free(b);
    validate();
    before = grows;
    a = allocate(2800, 16);
    assert(grows == before);
    heap_free(a); heap_free(d); empty();
    /* Partial trim must preserve live data and allow regrowth. */
    a = allocate(100, 16); b = allocate(100000, 4096);
    before = mapped;
    heap_free(b);
    assert(mapped < before && mapped > 0);
    for (size_t i = 0; i < 100; ++i) assert(((unsigned char *)a)[i] == 0x5a);
    b = allocate(200000, 64);
    heap_free(a); heap_free(b); empty();
    /* No-split path and extension after an allocated last block. */
    a = allocate(1, 16);
    size_t overhead = sizeof(HeapBlock *) + 15;
    b = allocate(heap_last->size - overhead, 16);
    assert(!heap_last->free);
    c = allocate(5000, 16);
    heap_free(a); heap_free(b); heap_free(c); empty();
    /* Exhaustion, failed-growth atomicity, then full recovery. */
    void *large[64]; size_t count = 0;
    while (count < 64 && (large[count] = heap_alloc(1024 * 1024))) ++count;
    assert(count > 0 && count < 64);
    validate();
    while (count) heap_free(large[--count]);
    empty();
    puts("deterministic cases: PASS");
#endif
}

void kmain(uint32_t mb_magic, const struct multiboot_info* mb_info)
{
    early_malloc_init();

    vga_init();
    printf("RastapopoulOS kernel running\n");
    if (mb_magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        printf("PANIC: Unsupported bootloader\n");
        return;
    }
    dump_multiboot(mb_info);

    struct Gdt gdt = {0};
    gdt_set_entry(&gdt.null, 0, 0, 0, 0);
    gdt_set_entry(&gdt.code, 0, 0xfffff, GDT_ACCESS_CODE, GDT_FLAGS_32BIT_4K);
    gdt_set_entry(&gdt.data, 0, 0xfffff, GDT_ACCESS_DATA, GDT_FLAGS_32BIT_4K);
    gdt_set_entry(&gdt.usercode, 0, 0xfffff, GDT_ACCESS_USERCODE,
                  GDT_FLAGS_32BIT_4K);
    gdt_set_entry(&gdt.userdata, 0, 0xfffff, GDT_ACCESS_USERDATA,
                  GDT_FLAGS_32BIT_4K);

    struct Gdtr gdtr = {.limit = sizeof(gdt) - 1,
                        .base = (uint32_t)(uintptr_t)&gdt};
    gdt_load(&gdtr);
    idt_init();

    struct multiboot_mmap_entry* mmap_entries =
        (struct multiboot_mmap_entry*)mb_info->mmap_addr;
    size_t mmap_len = mb_info->mmap_length;
    if (mmap_len == 0) {
        mmap_len = sizeof(struct multiboot_mmap_entry) * 2;
        mmap_entries = early_malloc(mmap_len);

        mmap_entries[0].size = sizeof(struct multiboot_mmap_entry);
        mmap_entries[0].addr = 0;
        mmap_entries[0].len = mb_info->mem_lower * 1024;
        mmap_entries[0].type = MULTIBOOT_MEMORY_AVAILABLE;

        mmap_entries[1].size = sizeof(struct multiboot_mmap_entry);
        mmap_entries[1].addr = 0x100000; /* 1Mb */
        mmap_entries[1].len = mb_info->mem_upper * 1024;
        mmap_entries[1].type = MULTIBOOT_MEMORY_AVAILABLE;
    }
    pmm_init(mmap_entries, mmap_len);
    printf("Free memory: %zu bytes\n", pmm_info());

    if (!vmm_init())
        panic("vmm_init failed");
    if (!vmm_map((void*)VGA_BASE, (void*)VGA_BASE, VMM_WRITABLE))
        panic("vmm_map failed");

    if (!heap_init())
        panic("heap_init failed");
    test_heap();
    printf("OK\n");
}

