#include "gdt.h"
#include "idt.h"
#include "kbd.h"
#include "pic.h"
#include "pit.h"
#include "pmm.h"
#include "vmm.h"
#include <debug.h>
#include <multiboot.h>
#include <serial.h>
#include <string.h>
#include <util.h>
#include <vga.h>

#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

extern unsigned char _heap_start[];
static unsigned char* _heap = _heap_start;
static struct multiboot_info multiboot_info;
bool early_kmalloc_enabled = true;

void* early_kmalloc_get_heap()
{
    return _heap;
}

void* early_kmalloc_aligned(size_t size, size_t alignment)
{
    if(!early_kmalloc_enabled) {
        PANIC("Invalid call to ealy_kmalloc");
    }
    uintptr_t result = ALIGN((uintptr_t)_heap, alignment);
    _heap = (unsigned char*)(result + size);
    return (void*)result;
}

void* early_kmalloc(size_t size)
{
    return early_kmalloc_aligned(size, 16);
}

static void handle_int80(struct isr_regs* regs)
{
    TRACE("int 0x80 called");
}

static void handle_page_fault(struct isr_regs* regs)
{
    uint32_t cr2 = read_cr2();
    TRACE("Page fault for %p", cr2);
    TRACE("CS: 0x%X, EIP: %p, ESP: %p", regs->cs, regs->eip, regs->esp);

#define PF_P        (1 << 0)
#define PF_WR       (1 << 1)
#define PF_US       (1 << 2)
#define PF_RSVD     (1 << 3)
#define PF_ID       (1 << 4)
    char info[16];
    if(regs->err_code & PF_P)
        TRACE("    - Page-level protection violation");
    else
        TRACE("    - Non-present page");
    if(regs->err_code & PF_WR)
        TRACE("    - Write");
    else
        TRACE("    - Read");
    if(regs->err_code & PF_US)
        TRACE("    - User mode");
    else
        TRACE("    - Supervisor mode");
    if(regs->err_code & PF_RSVD)
        TRACE("    - Reserved bits set to 1");
    if(regs->err_code & PF_ID)
        TRACE("    - Instuction fetch");
    HALT();
}

static void handle_gpf(struct isr_regs* regs)
{
    TRACE("General protection fault at 0x%X:%p", regs->cs, regs->eip);
    HALT();
}

void trace_init()
{
    serial_write_char('\n');
}

bool debug_write(char ch, void*)
{
    serial_write_char(ch);
    vga_write_char(ch, COLOR_LIGHTGRAY);
    return true;
}

void trace(const char* file, int line, const char* fn, const char* fmt, ...)
{
    char prefix[64];
    snprintf(prefix, sizeof(prefix), "[%s:%s:%d] ", basename(file), fn, line);
    serial_write_string(prefix);
    vga_write_string(prefix, COLOR_LIGHTGRAY);

    va_list args;
    va_start(args, fmt);
    formatv(debug_write, NULL, fmt, args);
    va_end(args);
    
    debug_write('\n', NULL);
}

void panic(const char* file, int line, const char* fn, const char* fmt, ...)
{
    trace(file, line, fn, "*** KERNEL PANIC ***");
    va_list args;
    va_start(args, fmt);
    formatv(debug_write, NULL, fmt, args);
    va_end(args);
    debug_write('\n', NULL);
    HALT();
}

static void stack_overflow(int id)
{
    TRACE("id: %d", id);
    unsigned char large[128];
    memset(large, 0, sizeof(large));
    if(id)
        stack_overflow(id - 1);
}

void kmain(const struct multiboot_info* multiboot, uint32_t multiboot_magic)
{
    vga_init();
    trace_init();
    gdt_init();

    /* Save multiboot information elsewhere before we manage to overwrite it */
    if(multiboot_magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        TRACE("PANIC: Bad multiboot magic");
    }
    memcpy(&multiboot_info, multiboot, sizeof(multiboot_info));
    if(multiboot_info.flags & MULTIBOOT_INFO_MEM_MAP) {
        struct multiboot_mmap_entry* mmap_entries = early_kmalloc(multiboot_info.mmap_length);
        memcpy(mmap_entries,
               multiboot_info.mmap_addr,
               multiboot_info.mmap_length);
        multiboot_info.mmap_addr = mmap_entries;
    }

    /*
     * Display memory map
     * multiboot_mmap_entry->size does not include the `size` member,
     * so we must add the size of an uint32_t while skipping to the next
     * entry
     */
    TRACE("Memory map:");
    TRACE("    ADDR       LEN        TYPE");
    MULTIBOOT_MMAP_ITERATE(multiboot_info.mmap_addr, e, multiboot_info.mmap_length) {
        uint32_t addr = e->addr & 0xFFFFFFFF;
        uint32_t len = e->len & 0xFFFFFFFF;
        TRACE("    %p %p %p", addr, len, e->type);
    }

    /* setup IDT */
    idt_init();
    idt_add_handler(0x0C, handle_gpf, 3);
    idt_add_handler(0x0E, handle_page_fault, 3);
    idt_add_handler(0x80, handle_int80, 3);
    TRACE("After setting up IDT");
    asm volatile("int 0x80\n":::"memory");
    TRACE("After calling int 80");

    /* Init PMM */
    pmm_init(multiboot_info.mmap_addr, multiboot_info.mmap_length);

    /*
     * At this point, the PMM is ready, but we still need to mark
     * used memory regions as used, as vmm_init below uses the PMM
     * As a simplification, all the low memory region is marked used
     * for now
     */
    for(const struct multiboot_mmap_entry* e = multiboot_info.mmap_addr;
        (uintptr_t)e < (uintptr_t)multiboot_info.mmap_addr + multiboot_info.mmap_length;
        e = (const struct multiboot_mmap_entry*)((uintptr_t)e + e->size + sizeof(uint32_t)))
    {
        if(e->type == MULTIBOOT_MEMORY_AVAILABLE) {
            uint32_t start = ALIGN(e->addr & 0xFFFFFFFF, VMM_PAGESIZE);
            uint32_t size = ROUND(
                    (e->len & 0xFFFFFFFF) -
                    (start - (e->addr & 0xFFFFFFFF)),
                    VMM_PAGESIZE);
            for(uint32_t frame = start; frame < start + size; frame += VMM_PAGESIZE) {
                if(frame <= (uint32_t)_heap) { /* 1Mb */
                    pmm_set(frame);
                }
            }
        }
    }

    /* At this point, if we pmm_alloc(), we should get a page which is > _heap */
    uint32_t new_frame = pmm_alloc();
    TRACE("_heap: %p, new_frame: %p", _heap, new_frame);
    ASSERT(new_frame > (uintptr_t)_heap);
    pmm_free(new_frame);

    /* And if we pmm_alloc() again, we should get the same frame */
    ASSERT(pmm_alloc() == new_frame);
    pmm_free(new_frame);

    /* Init VMM */
    vmm_init(multiboot_info.mmap_addr, multiboot_info.mmap_length);

    /*
     * At this point, allocating memory involves pmm_alloc() and vmm_map()
     * Most notably, writing beyond ALIGN(_heap, VMM_PAGESIZE) will corrupt
     * pagetables and lead to strange bugs
     */
#define CURRENT_TEST 0
#if CURRENT_TEST == 1
    // Write into read-only page
    TRACE("Testing write into read-only page at 0x00103D00");
    uint32_t* ptr = (uint32_t*)0x00103D00;
    *ptr = 0xDEADBEEF;
#elif CURRENT_TEST == 2
    // Write into non-present page
    TRACE("Testing write into non-present page");
    uint32_t* ptr = (uint32_t*)ALIGN((uintptr_t)_heap, VMM_PAGESIZE);
    DUMPP(ptr);
    *ptr = 0xDEADBEEF;
#elif CURRENT_TEST == 3
    // Map non-present page and write into it
    TRACE("Testing vmm_map");
    uint32_t* ptr = (uint32_t*)ALIGN((uintptr_t)_heap, VMM_PAGESIZE);
    uint32_t frame = pmm_alloc();
    TRACE("ptr: %p, frame: %p", ptr, frame);
    if(!vmm_map(ptr, frame, VMM_PTE_WRITABLE))
        PANIC("vmm_map failed");
    TRACE("*ptr: %p", *ptr);
#elif CURRENT_TEST == 4
    // Unmap normally-present page and write to it
    TRACE("Testing vmm unmap");
    uint32_t* ptr = (uint32_t*)0xDEADB000;
    uint32_t frame = pmm_alloc();
    TRACE("ptr: %p, frame: %p", ptr, frame);
    if(!vmm_map(ptr, frame, VMM_PTE_WRITABLE))
        PANIC("vmm_map failed");
    *ptr = 0xDEADBEEF;
    if(!vmm_unmap(ptr))
        PANIC("vmm_unmap failed");
    *ptr = 0xDEADBEEF;
#elif CURRENT_TEST == 5
    TRACE("Testing kernel stack overflow protection");
    stack_overflow(40);
#endif

    /* Setup PIC */
    pic_init();

    /* Setup PIT */
    pit_init();

    /* Add keyboard handler */
    kbd_init();

#ifdef UNIT_TESTS
    /* Run unit tests */
    void bitset_run_tests();
    add_test("bitset", bitset_run_tests);
    run_tests();
#endif

    /* Enable interrupts */
    TRACE("Waiting for an interrupt");
    asm volatile("sti":::"memory");

    /* Wait for an interrupt */
    while(1) {
        asm volatile("hlt":::"memory");
    }
}

