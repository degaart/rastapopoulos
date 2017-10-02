#include "kernel.h"
#include "io.h"
#include "string.h"
#include "kdebug.h"
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
#include "syscall_handler.h"
#include "ipc.h"
#include "list.h"
#include "heap.h"
#include "elf.h"
#include "debug.h"
#include "initrd.h"
#include "scheduler.h"

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

#include "int10_stub.h"
struct int10_regs {
    uint32_t eax, ebx, ecx, edx;
    uint32_t ebp, esi, edi;
    uint32_t es, fs, gs;
    uint32_t reserved;
} __attribute((packed));
void int10(struct int10_regs*);
struct coord {
    int row, col;
};
static struct coord get_cursor_pos()
{
    struct int10_regs regs = { .eax = 0x0300 };
    int10(&regs);

    struct coord result;
    result.row = (regs.edx & 0xFF00) >> 8;
    result.col = regs.edx & 0xFF;
    return result;
}

static void set_cursor_pos(int x, int y)
{
    /*
     * AH = 02h
     * BH = page number
     * 0-3 in modes 2&3
     * 0-7 in modes 0&1
     * 0 in graphics modes
     * DH = row (00h is top)
     * DL = column (00h is left)
     */
    struct int10_regs regs = {
        .eax = 0x0200,
        .ebx = 0,
        .edx = ((y & 0xFF) << 8) | (y & 0xFF)
    };
    int10(&regs);
}

static void write_char(int ch, int row, int col)
{
    /*
        AH = 0Ah
        AL = character to display
        BH = page number (00h to number of pages - 1) (see #00010)
        background color in 256-color graphics modes (ET4000)
        BL = attribute (PCjr, Tandy 1000 only) or color (graphics mode)
        if bit 7 set in <256-color graphics mode, character is XOR'ed
        onto screen
        CX = number of times to write character

        Return:
        Nothing
     */
    struct int10_regs regs;
    regs.eax = 0x0E | (ch & 0xFF);
    regs.ebx = 0x0007;
    int10(&regs);
}

static void write_string(const char* str, int attribute, int row, int col)
{
    /*
        VIDEO - WRITE STRING (AT and later,EGA)

        AH = 13h
        AL = write mode

        bit 0:
        Update cursor after writing

        bit 1:
        String contains alternating characters and attributes

        bits 2-7:
        Reserved (0).
        BH = page number.
        BL = attribute if string contains only characters.
        CX = number of characters in string.
        DH,DL = row,column at which to start writing.
        ES:BP -> string to write
     */
    size_t len = strlen(str) & 0xFF; /* 256 chars limit */
    struct int10_regs regs = {
        .eax = 0x1301,
        .ebx = attribute & 0xFF,
        .ecx = len,
        .edx = ((row & 0xFF) << 8) | (col & 0xFF),
        .ebp = 0x7b00,
        .es = 0,
    };
    memcpy((void*)0x7b00, str, len);
    int10(&regs);
}

void test_int10()
{
    trace("Testing int10 calls");

    /* Copy stub to 0x7C00 */
    memcpy((void*)0x7C00, obj_int10_stub_bin, sizeof(obj_int10_stub_bin));

    struct coord cursor = get_cursor_pos();
    trace("row: %d, col: %d", cursor.row, cursor.col);
    write_string("It works!", 2, cursor.row, cursor.col);
    while(1);
}

void kmain(struct multiboot_info* init_multiboot_info)
{
    /*
     * Init debug/loggin functions
     */
    kdebug_init();

    trace("*** Booted ***");
 
    /*
     * This function will adjust multiboot info structure addresses
     * and return the highest address it uses
     */
    unsigned char* multiboot_end = multiboot_init(init_multiboot_info);
    trace("multiboot_end: %p", multiboot_end);
    if(multiboot_end >= (unsigned char*)0xC0400000)
        panic("Multiboot data too large to fit into initial kernel memory");

    // Init kernel heap
    kmalloc_init(multiboot_end);

    // Load symbols
    load_symbols(multiboot_get_info());
    
    // GDT
    gdt_init();

    // IDT
    idt_init();
    idt_flush();
    idt_install(14, pf_handler, true);
    idt_install(13, gpf_handler, true);
    
    // test int10
    test_int10();

    // Physical memory manager
    pmm_init(multiboot_get_info());

    // initrd
    initrd_init(multiboot_get_info());

    /*
     * Reserve currently used memory
     * kernel code and data section
     * initial kernel heap
     */
    struct kernel_heap_info heap_info;
    kernel_heap_info(&heap_info);

    trace("Kernel map:");
    trace("\t.text   %p - %p", _TEXT_START_, _TEXT_END_);
    trace("\t.rodata %p - %p", _RODATA_START_, _RODATA_END_);
    trace("\t.data   %p - %p", _DATA_START_, _DATA_END_);
    trace("\t.bss    %p - %p", _BSS_START_, _BSS_END_);
    trace("\theap    %p - %p", heap_info.heap_start, heap_info.heap_start + heap_info.heap_size);

    /*
     * Reserve entire lower 1MB of memory so we can use vm86/swith to real mode for
     * video bios calls
     */
    for(uint32_t page = 0x00000000; page < 0x100000; page += PAGE_SIZE) {
        if(pmm_exists(page))
            pmm_reserve(page);
    }

    /* Kernel code+data */
    for(unsigned char* page = (unsigned char*)TRUNCATE((uint32_t)_KERNEL_START_ - KERNEL_BASE_ADDR, PAGE_SIZE); 
        page < _KERNEL_END_ - KERNEL_BASE_ADDR; 
        page += PAGE_SIZE) {
        if(pmm_exists((uint32_t)page))
            pmm_reserve((uint32_t)page);
    }

    /* Kernel heap */
    for(uint32_t page = TRUNCATE(heap_info.heap_start, PAGE_SIZE) - KERNEL_BASE_ADDR; 
        page < heap_info.heap_start + heap_info.heap_size - KERNEL_BASE_ADDR; 
        page += PAGE_SIZE) {

        if(pmm_exists(page) && !pmm_reserved(page))
            pmm_reserve(page);
    }

    // Virtual memory manager
    vmm_init();
    // By this point, multiboot data is not valid anymore

    // PIC
    pic_init();

    // System timer
    timer_init();

    // Syscall handlers
    syscall_init();

    // IPC System
    ipc_init();

    // Start system
    scheduler_start();

    reboot();
}




