#include "kernel.h"
#include "io.h"
#include "string.h"
#include "debug.h"
#include "registers.h"
#include "multiboot.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "timer.h"
#include "pmm.h"
#include "util.h"
#include "kmalloc.h"
#include "vmm.h"
#include "syscall.h"
#include "ipc.h"

uint32_t KERNEL_START = (uint32_t) &_KERNEL_START_;
uint32_t KERNEL_END = (uint32_t) &_KERNEL_END_;

const char* current_task_name();
int current_task_pid();

static void pf_handler(struct isr_regs* regs)
{
    bool user_mode = regs->err_code & (1 << 2); /* user or supervisor mode */
    bool write = regs->err_code & (1 << 1);     /* was a read or a write */
    bool prot_violation = regs->err_code & 1;   /* not-present page or page protection violation */
    void* address = (void*)read_cr2();
    const char* function = lookup_function(regs->eip);

    trace(
        "Page fault at address %p: %s %s %s\n"
        "\tds:  0x%X\n"
        "\teax: 0x%X ebx: 0x%X ecx: 0x%X edx: 0x%X\n"
        "\tesi: 0x%X edi: 0x%X\n"
        "\terr: 0x%X\n"
        "\tcs:  0x%X eip: 0x%X (%s) eflags: 0x%X\n"
        "\tss:  0x%X esp: 0x%X\n"
        "\tcr3: %p\n"
        "\tcurrent task: %d %s\n",
        address,
        user_mode ? "ring3" : "ring0",
        write ? "write" : "read",
        prot_violation ? "access-violation" : "page-not-present",
        regs->ds,
        regs->eax, regs->ebx, regs->ecx, regs->edx,
        regs->esi, regs->edi,
        regs->err_code,
        regs->cs, regs->eip, function ? function : "??", regs->eflags, 
        regs->ss, regs->esp,
        read_cr3(),
        current_task_pid(), current_task_name() ? current_task_name() : ""
    );
    abort();
}

static void gpf_handler(struct isr_regs* regs)
{
    const char* function = lookup_function(regs->eip);

    trace(
        "General protection fault:\n"
        "\tdescriptor: %p\n"
        "\tds:  0x%X\n"
        "\teax: 0x%X ebx: 0x%X ecx: 0x%X edx: 0x%X\n"
        "\tesi: 0x%X edi: 0x%X\n"
        "\tcs:  0x%X eip: 0x%X (%s) eflags: 0x%X\n"
        "\tss:  0x%X esp: 0x%X\n",
        regs->err_code,
        regs->ds,
        regs->eax, regs->ebx, regs->ecx, regs->edx,
        regs->esi, regs->edi,
        regs->cs, regs->eip, function ? function : "??", regs->eflags, 
        regs->ss, regs->esp
    );
    abort();
}

void reboot()
{
    trace("*** Rebooted ***\n\n\n");

    uint8_t good = 0x02;
    while (good & 0x02)
        good = inb(0x64);
    outb(0x64, 0xFE);
    halt();
}

void abort()
{
    backtrace();
    reboot();
}

void halt()
{
    cli();
    hlt();
}

static void reboot_timer(void* data, const struct isr_regs* regs)
{
    trace("*** Rebooting ***\n\n\n");
    reboot();
}

static void dump_multiboot_info(const struct multiboot_info* multiboot_info)
{
    trace("Multiboot info: %p", multiboot_info);

    char multiboot_flags[32] = {};
    if(multiboot_info->flags & MULTIBOOT_FLAG_MEMINFO) {
        strlcat(multiboot_flags, "MEM ", sizeof(multiboot_flags));
        trace("Lower memory size: %dk", multiboot_info->mem_lower);
        trace("High memory size: %dk", multiboot_info->mem_upper);
    }
    if(multiboot_info->flags & MULTIBOOT_FLAG_MODINFO) {
        strlcat(multiboot_flags, "MOD ", sizeof(multiboot_flags));
        trace("Modules count: %d", multiboot_info->mods_count);
        trace("Modules load address: %p", multiboot_info->mods_addr);
    }
    if(multiboot_info->flags & MULTIBOOT_FLAG_SYMBOLS1) {
        strlcat(multiboot_flags, "SYM1 ", sizeof(multiboot_flags));
    }
    if(multiboot_info->flags & MULTIBOOT_FLAG_SYMBOLS2) {
        strlcat(multiboot_flags, "SYM2 ", sizeof(multiboot_flags));

        load_symbols(multiboot_info);
    }
    if(multiboot_info->flags & MULTIBOOT_FLAG_MMAP) {
        strlcat(multiboot_flags, "MMAP ", sizeof(multiboot_flags));
        trace("Memory map len: %d", multiboot_info->mmap_len);
        trace("Memory map load address: %p", multiboot_info->mmap_addr);
    }

    trace("Multiboot flags: %s", multiboot_flags);
    trace("_KERNEL_END_: %p", &_KERNEL_END_);
}

#define RUN_TEST(fn) \
    do { \
        trace("Running test %s", #fn); \
        void fn(); \
        fn(); \
    } while(0)


static void run_tests()
{
    //RUN_TEST(test_vmm);
    //RUN_TEST(test_kmalloc);
    //RUN_TEST(test_bitset);
    //RUN_TEST(test_list);
    RUN_TEST(test_scheduler);
    //RUN_TEST(test_locks);
}

void kmain(const struct multiboot_info* multiboot_info)
{
    trace("*** Booted ***");

    // Init kernel heap
    kmalloc_init();

    // Load symbols
    load_symbols(multiboot_info);
    
    // GDT
    gdt_init();
    gdt_iomap_set(DEBUG_PORT, 0);

    // IDT
    idt_init();
    idt_flush();
    idt_install(14, pf_handler, true);
    idt_install(13, gpf_handler, true);

    // Physical memory manager
    pmm_init(multiboot_info);

    /*
     * Reserve currently used memory
     * Low memory: 0x00000000 - 0x000FFFFF
     * multiboot_info
     * kernel code and data section
     * initial kernel heap
     * TODO: Free conventional memory after we load multiboot modules
     */
    for(uint32_t page = 0; page < 0xFFFFF; page += PAGE_SIZE) {
        if(pmm_exists(page))
            pmm_reserve(page);
    }

    for(uint32_t page = TRUNCATE((uint32_t)multiboot_info, PAGE_SIZE); 
        page < (uint32_t)multiboot_info + sizeof(struct multiboot_info); 
        page += PAGE_SIZE) {
        if(pmm_exists(page) && !pmm_reserved(page))
            pmm_reserve(page);
    }

    for(uint32_t page = TRUNCATE(KERNEL_START, PAGE_SIZE); 
        page < KERNEL_END; 
        page += PAGE_SIZE) {
        if(pmm_exists(page))
            pmm_reserve(page);
    }

    struct kernel_heap_info heap_info;
    kernel_heap_info(&heap_info);
    for(uint32_t page = TRUNCATE(heap_info.heap_start, PAGE_SIZE); 
        page < heap_info.heap_start + heap_info.heap_size; 
        page += PAGE_SIZE) {

        if(pmm_exists(page) && !pmm_reserved(page))
            pmm_reserve(page);
    }

    // Virtual memory manager
    vmm_init();

    // PIC
    pic_init();

    // System timer
    timer_init();

    // Syscall handlers
    syscall_init();

    // IPC System
    ipc_init();

    // Run tests
    run_tests();
    reboot();
}




