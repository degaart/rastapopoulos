#include "early_malloc.h"
#include "heap.h"
#include "idt.h"
#include "kernel.h"
#include "kmalloc.h"
#include "kthread.h"
#include "pic.h"
#include "pit.h"
#include "pmm.h"
#include "ps2kbd.h"
#include "serial.h"
#include "sysinfo.h"
#include "trace.h"
#include "vga.h"
#include "vmm.h"
#include <assert.h>
#include <gdt.h>
#include <multiboot.h>
#include <stdio.h>
#include <string.h>

struct TSS tss;

static void gpf_handler(int, struct isr_regs* regs)
{
    uint32_t ext = regs->error_code & 1;
    uint32_t idt = (regs->error_code >> 1) & 1;
    uint32_t ti = (regs->error_code >> 2) & 1;
    uint32_t desc = regs->error_code & ~7u;
    uint32_t cr0 = read_cr0();
    uint32_t ss = read_ss();

    panic("General protection fault:\n"
          "    ext: %lu idt: %lu ti: %lu desc: 0x%lx\n"
          "    eip: 0x%lx cs: 0x%lx eflags: 0x%lx\n"
          "    esp: 0x%lx ss: 0x%lx\n"
          "    cr0: 0x%lx\n"
          "    eax: 0x%lx ebx: 0x%lx ecx: 0x%lx edx: 0x%lx\n"
          "    esi: 0x%lx edi: 0x%lx ebp: 0x%lx\n",
          ext, idt, ti, desc, regs->eip, regs->cs, regs->eflags, regs->esp, ss,
          cr0, regs->eax, regs->ebx, regs->ecx, regs->edx, regs->esi,
          regs->edi, regs->ebp);
}

static void dump_multiboot(const struct multiboot_info* info)
{
    trace("Multiboot info:");
    if (info->flags & MULTIBOOT_INFO_MEMORY) {
        trace("    mem_lower: 0x%lx, mem_upper: 0x%lx", info->mem_lower,
              info->mem_upper);
    }
    if (info->flags & MULTIBOOT_INFO_BOOTDEV) {
        trace("    boot_device: 0x%lx", info->boot_device);
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

            trace("    mmap: 0x%08llx-0x%08llx 0x%08llx %s", entry->addr,
                  entry->addr + entry->len - 1, entry->len, type);
        }
    }
    if (info->flags & MULTIBOOT_INFO_BOOT_LOADER_NAME) {
        trace("    bootloader: %s", (char*)info->boot_loader_name);
    }
}

void test_heap(void)
{
    trace("Free mem before tests: %zu bytes", pmm_info());

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
            void* p = kmemalign(a, sizes[i]);
            if (p)
                memset(p, 0, sizes[i]);
            kfree(p);
        }
    }

    trace("Free mem after tests: %zu bytes", pmm_info());

    struct HeapInfo hi = heap_info();
    trace("Heap info:\n"
          "    total:      %zu\n"
          "    free:       %zu\n"
          "    allocated:  %zu\n"
          "    overhead:   %zu",
          hi.total_size, hi.free_size, hi.allocated_size, hi.overhead);
}

static void tss_load(uint16_t tss_seg)
{
    asm volatile("ltr %0" : : "r"(tss_seg) : "memory");
}

static void thread1_entry(void* param)
{
    trace("Thread1 started");
    if ((read_eflags() & EFLAGS_IF) == 0)
        panic("IF=0");
    char buffer[16];
    int counter = 0;
    int y = 10;
    while (pit_get_ticks() < 1000) {
        int len = uint32_to_string(counter, buffer, sizeof(buffer));
        int x = (VGA_WIDTH - len) / 2;
        for (int i = 0; i < len; i++) {
            vga_write_at(x + i, y, buffer[i], VGA_COLOR_WHITE,
                         VGA_COLOR_GREEN);
        }
        counter++;
    }
    trace("Thread1 exited");
}

static void thread2_entry(void* param)
{
    trace("Thread2 started");
    char buffer[16];
    int counter = 0;
    int y = 11;
    while (pit_get_ticks() < 2000) {
        int len = uint32_to_string(counter, buffer, sizeof(buffer));
        int x = (VGA_WIDTH - len) / 2;
        for (int i = 0; i < len; i++) {
            vga_write_at(x + i, y, buffer[i], VGA_COLOR_DARK_GREY,
                         VGA_COLOR_YELLOW);
        }
        counter++;
    }
    trace("Thread2 exited");
}

static void thread3_entry(void* param)
{
    trace("Thread3 started");
    char buffer[16];
    int counter = 0;
    int y = 12;
    while (pit_get_ticks() < 3000) {
        int len = uint32_to_string(counter, buffer, sizeof(buffer));
        int x = (VGA_WIDTH - len) / 2;
        for (int i = 0; i < len; i++) {
            vga_write_at(x + i, y, buffer[i], VGA_COLOR_BLACK, VGA_COLOR_CYAN);
        }
        counter++;
    }
    trace("Thread3 exited");
}

static void thread4_entry(void* param)
{
    trace("Thread4 started");
    char buffer[16];
    int counter = 0;
    int y = 13;
    while (pit_get_ticks() < 4000) {
        int len = uint32_to_string(counter, buffer, sizeof(buffer));
        int x = (VGA_WIDTH - len) / 2;
        for (int i = 0; i < len; i++) {
            vga_write_at(x + i, y, buffer[i], VGA_COLOR_BLACK, VGA_COLOR_RED);
        }
        counter++;
    }
    trace("Thread4 exited");
}

extern uint8_t usercode_start[];
extern uint8_t usercode_end[];

#define USER_BASE  0x00400000
#define USER_STACK 0xc0000000

static void user_entry(void* param)
{
    trace("Jumping to ring3");
    ring3_jump((void*)USER_BASE, (void*)(USER_STACK + VMM_PAGE_SIZE),
               USERCODESEG | 0x3, USERDATASEG | 0x3);
    trace("Here");
}

void kmain(uint32_t mb_magic, const struct multiboot_info* mb_info)
{
    serial_init();
    trace("Kernel entered");
    early_malloc_init();

    vga_init();
    printf("RastapopoulOS kernel running\n");
    if (mb_magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        panic("Unsupported bootloader");
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
    gdt_set_entry(&gdt.tss, (uintptr_t)&tss, sizeof(tss) - 1, GDT_ACCESS_TSS,
                  GDT_FLAGS_TSS);

    memset(&tss, 0, sizeof(tss));
    tss.ss0 = DATASEG;
    tss.iomap_base = sizeof(struct TSS);

    struct Gdtr gdtr = {.limit = sizeof(gdt) - 1,
                        .base = (uint32_t)(uintptr_t)&gdt};
    gdt_load(&gdtr);
    tss_load(TSSSEG);
    idt_init();
    idt_set_handler(0xd, gpf_handler);
    trace("IDT installed");

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
    trace("PMM initialized");
    trace("Free memory: %zu bytes", pmm_info());

    if (!vmm_init())
        panic("vmm_init failed");
    if (!vmm_map((void*)VGA_BASE, (void*)VGA_BASE, VMM_WRITABLE))
        panic("vmm_map failed");

    if (!heap_init())
        panic("heap_init failed");
    trace("Heap initialized");

    pic_init();
    pit_init(100);

    ps2kbd_init();

    thread_init();
    thread_create("thread1", thread1_entry, NULL);
    thread_create("thread2", thread2_entry, NULL);
    thread_create("thread3", thread3_entry, NULL);
    thread_create("thread4", thread4_entry, NULL);
    sysinfo_init();

    void* frame = pmm_alloc();
    if (!frame)
        panic("Out of memory");

    /* Code */
    if (!vmm_map(frame, (void*)USER_BASE, VMM_USER))
        panic("Failed to map page");

    uint8_t* ptr = (void*)USER_BASE;
    memset(ptr, 0xcc, VMM_PAGE_SIZE);

    size_t usercode_len = usercode_end - usercode_start;
    assert(usercode_len <= VMM_PAGE_SIZE);
    memcpy(ptr, usercode_start, usercode_len);

    /* Stack */
    frame = pmm_alloc();
    if (!frame)
        panic("Out of memory");

    if (!vmm_map(frame, (void*)USER_STACK, VMM_USER | VMM_WRITABLE))
        panic("Failed to map page");
    ptr = (void*)USER_STACK;
    memset(ptr, 0xcc, VMM_PAGE_SIZE);

    /* VGA mem */
    if (!vmm_remap((void*)VGA_BASE, VMM_USER | VMM_WRITABLE))
        panic("Failed to remap page");

    thread_create("user", user_entry, NULL);

    schedule();
    trace("Scheduler ready: IF=%u, PIC mask=0x%x, IRR=0x%x, ISR=0x%x",
          (read_eflags() & EFLAGS_IF) != 0, inb(0x21), pic_get_irr(),
          pic_get_isr());
    enable_interrupts();
    trace("Idle thread resumed: ticks=%llu", pit_get_ticks());
    uint32_t idle_counter = 0;
    while (1) {
        char buf[16];
        int len = uint32_to_string(idle_counter++, buf, sizeof(buf));
        for (int i = 0; i < len; i++) {
            vga_write_at(VGA_WIDTH + i - len, 1, buf[i], VGA_COLOR_LIGHT_GREY,
                         VGA_COLOR_BLACK);
        }

        if ((read_eflags() & EFLAGS_IF) == 0)
            panic("Interrupts disabled in idle thread");
        hlt();
    }
    panic("Idle thread exited");
}

