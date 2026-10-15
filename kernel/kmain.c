#include "bitset.h"
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
#include "syscall.h"
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

static void tss_load(uint16_t tss_seg)
{
    asm volatile("ltr %0" : : "r"(tss_seg) : "memory");
}

extern uint8_t usercode_start[];
extern uint8_t usercode_end[];
extern uint8_t ring3_trampoline[];
extern uint8_t ring3_trampoline_end[];

#define RING3_TRAMPOLINE_BASE 0x1000
#define USER_BASE             0x00400000
#define USER_STACK            (KERNEL_OFFSET - VMM_PAGE_SIZE)

static void user_entry(void* param)
{
    trace("Jumping to ring3");

    /* Map ring3 trampoline */
    void* frame = pmm_alloc();
    if (!frame)
        panic("Out of memory");
    if (!vmm_map(frame, (void*)RING3_TRAMPOLINE_BASE, VMM_USER))
        panic("Failed to map page 0x%x", RING3_TRAMPOLINE_BASE);
    memcpy((void*)RING3_TRAMPOLINE_BASE, ring3_trampoline,
           ring3_trampoline_end - ring3_trampoline);

    /* Map user code + data + bss */
    size_t usercode_len = usercode_end - usercode_start;
    for (uintptr_t ptr = USER_BASE; ptr < USER_BASE + usercode_len;
         ptr += VMM_PAGE_SIZE) {
        frame = pmm_alloc();
        if (!frame)
            panic("Out of memory");
        if (!vmm_map(frame, (void*)ptr, VMM_USER | VMM_WRITABLE))
            panic("Failed to map page 0x%lx", ptr);
    }

    uint8_t* ptr = (void*)USER_BASE;
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
    if (!vmm_map((void*)VGA_PHYSICAL_BASE, (void*)VGA_PHYSICAL_BASE,
                 VMM_USER | VMM_WRITABLE))
        panic("Failed to map page 0x%x", VGA_PHYSICAL_BASE);

    uint32_t esp = USER_STACK + VMM_PAGE_SIZE;
    ring3_jump((void*)RING3_TRAMPOLINE_BASE,
               (void*)(USER_STACK + VMM_PAGE_SIZE), USERCODESEG | 0x3,
               USERDATASEG | 0x3);
    panic("ring3_jump returned");
}

void kmain(uint32_t mb_magic, const struct multiboot_info* mb_info)
{
    serial_init();
    trace("Kernel entered");
    trace("mb_magic: 0x%lx, mb_info: 0x%p", mb_magic, mb_info);
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
    idt_set_handler(0xd, gpf_handler, 0);
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

    /* After vmm initialization, lower-half is unmapped */
    if (!vmm_init())
        panic("vmm_init failed");
    if (!vmm_map((void*)VGA_PHYSICAL_BASE, (void*)VGA_BASE, VMM_WRITABLE))
        panic("vmm_map failed");

    mb_info = NULL;
    mmap_entries = NULL;

    /* Now we can free the bootstrap frames */
    for (void* frame = __bootstrap_start; frame < (void*)__bootstrap_end;
         frame += PMM_FRAME_SIZE) {
        pmm_free(frame);
    }

    if (!heap_init())
        panic("heap_init failed");
    trace("Heap initialized");

    pic_init();
    pit_init(100);

    ps2kbd_init();
    syscall_init();

    thread_init();
    thread_create("user", user_entry, NULL);

    schedule();
    trace("Scheduler ready: IF=%u, PIC mask=0x%x, IRR=0x%x, ISR=0x%x",
          (read_eflags() & EFLAGS_IF) != 0, inb(0x21), pic_get_irr(),
          pic_get_isr());
    enable_interrupts();
    trace("Idle thread resumed: ticks=%llu", pit_get_ticks());

    while (1) {
        if ((read_eflags() & EFLAGS_IF) == 0)
            panic("Interrupts disabled in idle thread");
        hlt();
    }
    panic("Idle thread exited");
}

