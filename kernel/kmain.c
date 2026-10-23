#include "address_space.h"
#include "bitset.h"
#include "early_malloc.h"
#include "exec.h"
#include "heap.h"
#include "idt.h"
#include "kernel.h"
#include "kmalloc.h"
#include "kthread.h"
#include "pic.h"
#include "pit.h"
#include "pmm.h"
#include "process.h"
#include "ps2kbd.h"
#include "psp.h"
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

static void segment_error_info(char* buffer, size_t len, unsigned error_code)
{
    uint32_t ext = error_code & 1;
    uint32_t idt = (error_code >> 1) & 1;
    uint32_t ti = (error_code >> 2) & 1;
    uint32_t desc = error_code & ~7u;
    snprintf(buffer, len, "%s %s desc: 0x%lx", ext ? "EXT" : "",
             idt ? "IDT" : (ti ? "LDT" : "GDT"), desc);
}

static void page_fault_info(char* buffer, size_t len, unsigned error_code)
{
    bool protect = error_code & (1 << 0);
    bool write = error_code & (1 << 1);
    bool user = error_code & (1 << 2);
    bool rsvd = error_code & (1 << 3);
    bool ins = error_code & (1 << 4);
    snprintf(buffer, len, "   %s %s %s %s %s",
             protect ? "ACCESS_VIOLATION" : "UNMAPPED_PAGE",
             write ? "WRITE" : "READ", user ? "USER" : "SUPERVISOR",
             rsvd ? "RSVD" : "", ins ? "INSTRUCTION_FETCH" : "DATA_FETCH");
}

static const char* fault_name(int num)
{
#define X(c)                                                                  \
    case c:                                                                   \
        return (#c) + 4;
    switch (num) {
        X(INT_DIVIDE_ERROR);
        X(INT_DEBUG_EXCEPTION);
        X(INT_NMI);
        X(INT_BREAKPOINT);
        X(INT_OVERFLOW);
        X(INT_BOUND_RANGE);
        X(INT_INVALID_OPCODE);
        X(INT_DEVICE_UNAVAIL);
        X(INT_DOUBLE_FAULT);
        X(INT_INVALID_TSS);
        X(INT_SEGMENT_NOT_PRESENT);
        X(INT_STACK_SEGMENT_FAULT);
        X(INT_GPF);
        X(INT_PF);
        X(INT_FP_EXCEPTION);
        X(INT_ALIGNMENT_CHECK);
        X(INT_MACHINE_CHECK);
        X(INT_SIMD_FP_EXCEPTION);
        X(INT_VIRT_EXCEPTION);
        X(INT_CONTROL_PROTECTION);
        X(INT_HYPERVISOR_INJECTION);
        X(INT_VMM_COMM_EXCEPTION);
        X(INT_SECURITY_EXCEPTION);
    }
    return "UNKNOWN";
#undef X
}

static void fault_handler(int num, struct isr_regs* regs)
{
    char info_buf[64] = {};
    const char* name = fault_name(num);
    bool usermode = GDT_DPL(regs->cs) != 0;
    switch (num) {
    case INT_INVALID_TSS:
    case INT_SEGMENT_NOT_PRESENT:
    case INT_STACK_SEGMENT_FAULT:
    case INT_GPF:
        segment_error_info(info_buf, sizeof(info_buf), regs->error_code);
        break;
    case INT_PF:
        page_fault_info(info_buf, sizeof(info_buf), regs->error_code);
        break;
    default:
        break;
    }

    char message_buf[192];
    char* info_ptr = info_buf;
    for (; *info_ptr && *info_ptr == ' '; info_ptr++)
        ;
    snprintf(message_buf, sizeof(message_buf),
             "Exception 0x%x %s pid %u %s\n"
             "   %s\n"
             "   eip: 0x%lx cs: 0x%lx esp: 0x%lx ss: 0x%lx cr2: 0x%lx",
             num, name, process_current()->pid, process_current()->name,
             info_ptr, regs->eip, regs->cs,
             usermode ? regs->user_esp : regs->esp,
             usermode ? regs->user_ss : read_ss(), read_cr2());

    trace("%s", message_buf);

    if (usermode) {
        process_exit(UINT8_MAX);
        return;
    }
    panic("%s", message_buf);
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

static void init_entry(void* unused)
{
    trace("Init thread started");
    while (true) {
        int ret = exec("shell", "");
        if (ret == -1)
            panic("Failed to exec shell");
        else if (ret != 0) {
            trace("Shell exited with status %d", ret);
            printf("Shell exited with status %d\n", ret);
        }
    }
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
    for (int i = 0; i < 32; i++) {
        int dpl = 0;
        if (i == INT_BREAKPOINT)
            dpl = 3;
        idt_set_handler(i, fault_handler, dpl);
    }
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

    /* After address_space initialization, lower-half is unmapped */
    address_space_init();
    if (!address_space_map(address_space_current(), (void*)VGA_PHYSICAL_BASE,
                           (void*)VGA_BASE, PAGE_WRITABLE))
        panic("Failed to map VGA_BASE");

    if (!vmm_init())
        panic("vmm_init failed");

    mb_info = NULL;
    mmap_entries = NULL;

    /* Now we can free the bootstrap frames */
    for (void* frame = __bootstrap_start; frame < (void*)__bootstrap_end;
         frame += PAGE_SIZE) {
        pmm_free(frame);
    }

    if (!heap_init())
        panic("heap_init failed");
    trace("Heap initialized");

    pic_init();
    pit_init(100);

    ps2kbd_init();
    syscall_init();

    process_init();
    thread_init();

    schedule();
    trace("Scheduler ready: IF=%u, PIC mask=0x%x, IRR=0x%x, ISR=0x%x",
          (read_eflags() & EFLAGS_IF) != 0, inb(0x21), pic_get_irr(),
          pic_get_isr());
    enable_interrupts();

    trace("Idle thread started: ticks=%llu", pit_get_ticks());

    if (!thread_create(process_current(), "init", init_entry, NULL)) {
        panic("Failed to create init thread");
    }

    while (1) {
        if ((read_eflags() & EFLAGS_IF) == 0)
            panic("Interrupts disabled in idle thread");
        process_cleanup_exited();
        hlt();
    }
    panic("Idle thread exited");
}

