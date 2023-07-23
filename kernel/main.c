#include "gdt.h"
#include "idt.h"
#include "kbd.h"
#include "kmalloc.h"
#include "pic.h"
#include "pit.h"
#include "pmm.h"
#include "vmm.h"
#include "../user/obj/program1.h"
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
    unsigned char* result = ALIGN_PTR(_heap, alignment);
    _heap = result + size;
    return result;
}

void* early_kmalloc(size_t size)
{
    return early_kmalloc_aligned(size, 16);
}

static void handle_page_fault(struct isr_regs* regs)
{
    uint32_t cr2 = read_cr2();
    TRACE("Page fault for 0x%08lX", cr2);
    TRACE("CS: 0x%lX, EIP: 0x%08lX, ESP: 0x%08lX", regs->cs, regs->eip, regs->esp);

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
    TRACE("General protection fault at 0x%lX:0x%08lX", regs->cs, regs->eip);
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

static void kmalloc_test()
{
    TRACE("kmalloc_heap: %p, kmalloc_heap_end: %p",
          kmalloc_heap, kmalloc_heap_end);

    /* Check kmalloc_heap is not mapped */
    struct vaddrinfo vi;
    vmm_vaddrinfo(&vi, kmalloc_heap);
    ASSERT(!(*vi.pde & VMM_PRESENT) || !(*vi.pte & VMM_PRESENT));

    /* Now malloc some stuff and free */
    uint32_t* ptr = kmalloc(sizeof(uint32_t));
    DUMPP(ptr);
    *ptr = 0xDEADBEEF;
    kfree(ptr);

    ptr = kmalloc(VMM_PAGESIZE * 8);
    memset(ptr, 0, VMM_PAGESIZE * 8);
    DUMPP(ptr);
    kfree(ptr);

    ptr = kmemalign(VMM_PAGESIZE * 8, VMM_PAGESIZE);
    DUMPP(ptr);
    kfree(ptr);

    ptr = kmalloc(sizeof(uint32_t));
    ptr[1] = 0xDEADBEEF;
    kfree(ptr);

    TRACE("footprint: 0x%lX", kmalloc_footprint());
    kmalloc_trim(0);
    TRACE("footprint: 0x%lX", kmalloc_footprint());

    void* ptrs[16];
    for(int i = 0; i < sizeof(ptrs)/sizeof(ptrs[0]); i++) {
        size_t size = (1 << i);
        TRACE("Allocating 0x%lX bytes", size);
        ptrs[i] = kmalloc(size);
    }

    for(int i = (sizeof(ptrs)/sizeof(ptrs[0])) - 1; i >= 0; i--) {
        kfree(ptrs[i]);
    }
    kmalloc_trim(0);
    TRACE("footprint: 0x%lX, kmalloc_heap: %p", kmalloc_footprint(), kmalloc_heap);
}

static void syscall0(struct isr_regs* regs)
{
    TRACE("%s", (const char*)regs->ebx);
}

static void syscall1(struct isr_regs* regs)
{
    regs->eax = regs->ebx + regs->ecx + regs->edx;
}

static void syscall2(struct isr_regs* regs)
{
    PANIC("Panic from usermode");
}

static void syscall3(struct isr_regs* regs)
{
    TRACE("Halt from usermode");
    HALT();
}

static void syscall4(struct isr_regs* regs)
{
    uint64_t ticks = get_ticks();
    regs->eax = ticks & 0xFFFFFFFF;
    regs->ebx = ticks >> 32;
}

static void syscall_handler(struct isr_regs* regs)
{
    //TRACE("Syscall handler called");
    //TRACE("esp: 0x%08lX", read_esp());
    //TRACE("IF: %s", interrupts_enabled() ? "SET" : "CLEAR");
    switch(regs->eax) {
        case 0:                 /* trace */
            syscall0(regs);
            break;
        case 1:                 /* add ebx+ecx+edx */
            syscall1(regs);
            break;
        case 2:                 /* panic */
            syscall2(regs);
            break;
        case 3:                 /* halt */
            syscall3(regs);
            break;
        case 4:                 /* getticks */
            syscall4(regs);
            break;
        default:
            PANIC("Invalid syscall 0x%02lX", regs->eax);
            break;
    }
}

static void test_usermode()
{
    idt_add_handler(0x30, syscall_handler, 3);

    /* Map program starting at 0x400000 */
    unsigned char* dst;
    const unsigned char* src;
    for(dst = (unsigned char*)0x400000, src = obj_program1_elf;
        src < obj_program1_elf + obj_program1_elf_len;
        dst += VMM_PAGESIZE, src += VMM_PAGESIZE)
    {
        if(!vmm_alloc(dst, VMM_WRITABLE | VMM_USER))
            PANIC("vmm_map failed");
        memcpy(dst, src, VMM_PAGESIZE);
    }

    /* Map its stack at 3G - 4096 */
    unsigned char* userstack = (unsigned char*)0xC0000000 - VMM_PAGESIZE;
    assert(IS_ALIGNED_PTR(userstack, VMM_PAGESIZE));
    if(!vmm_alloc(userstack, VMM_WRITABLE | VMM_USER))
        PANIC("vmm_alloc failed");

    unsigned char* kernelstack = kpvalloc(VMM_PAGESIZE);
    assert(IS_ALIGNED_PTR(kernelstack, VMM_PAGESIZE));
    tss_set_esp0(kernelstack);

    void user_entry(void);
    uint32_t esp = (uintptr_t)userstack + VMM_PAGESIZE;
    uint32_t eip = 0x00400000;
    TRACE("Entering usermode, esp: 0x%08lX, eip: 0x%08lX, esp0: %p",
          esp, eip, kernelstack);
    asm volatile(
            "xchg bx, bx\n"
            "cli\n"
            "mov   ax, 0x23\n"
            "mov   ds, ax\n"
            "mov   es, ax\n"
            "mov   fs, ax\n"
            "mov   gs, ax\n"
            "pushd 0x23\n"
            "pushd ebx\n"       /* esp */
            "pushf\n"
            "pop   eax\n"
            "or    eax, 0x200\n"    /* IF */
            "pushd eax\n"
            "pushd 0x18|0x3\n"
            "push  ecx\n"       /* eip */
            "iretd\n"
            ".next:\n"
            :
            : "ebx"(esp), "ecx"(eip));
    PANIC("Invalid code path");
}

struct task {
    /* 
     * This must be the first field as switch_task does not have a
     * definition of struct task
     */
    uint32_t        esp;
    void*           stack;
    struct task*    next;
};

struct task* current_task = NULL;
void switch_task(struct task*);

#define IMPLEMENT_TASK(name, index) \
    static void name ## _entry() \
    { \
        enable_interrupts(); \
        static uint16_t* vga_base = (uint16_t*)VGA_BASE; \
        unsigned counter = 0; \
        while(1) { \
            uint32_t ticks = get_ticks(); \
            counter++; \
            char buffer[64]; \
            snprintf(buffer, sizeof(buffer), "0x%08lX 0x%08X", ticks, counter); \
            uint16_t* d = vga_base + (index * VGA_WIDTH); \
            for(char* s = buffer; *s; s++, d++) { \
                *d = *s | (uint16_t)(0x1F << 8); \
            } \
        } \
    }

IMPLEMENT_TASK(task1, 0);
IMPLEMENT_TASK(task2, 1);
IMPLEMENT_TASK(task3, 2);
IMPLEMENT_TASK(task4, 3);
IMPLEMENT_TASK(task5, 4);
IMPLEMENT_TASK(task6, 5);
IMPLEMENT_TASK(task7, 6);
IMPLEMENT_TASK(task8, 7);
IMPLEMENT_TASK(task9, 8);
IMPLEMENT_TASK(task10, 9);
IMPLEMENT_TASK(task11, 10);
IMPLEMENT_TASK(task12, 11);
IMPLEMENT_TASK(task13, 12);
IMPLEMENT_TASK(task14, 13);
IMPLEMENT_TASK(task15, 14);
IMPLEMENT_TASK(task16, 15);
IMPLEMENT_TASK(task17, 16);
IMPLEMENT_TASK(task18, 17);
IMPLEMENT_TASK(task19, 18);
IMPLEMENT_TASK(task20, 19);

static void schedule_timer(uint64_t ticks, void* ctx)
{
    CLEAR_IF();
    switch_task(current_task->next);
    RESTORE_IF();
}

#define XCREATE_TASK(name) \
    struct task* name = kmalloc(sizeof(struct task)); \
    name->stack = kpvalloc(VMM_PAGESIZE); \
    esp = (uint32_t*)(name->stack + VMM_PAGESIZE); \
    *(--esp) = 0; \
    *(--esp) = (uintptr_t)name ## _entry; \
    *(--esp) = 0xDEADBEE0; \
    *(--esp) = 0xDEADBEE1; \
    *(--esp) = 0xDEADBEE2; \
    *(--esp) = 0xDEADBEE3; \
    name->esp = (uintptr_t)esp

#define CREATE_TASK(prev, name) \
    XCREATE_TASK(name); \
    prev->next = name

/*
 * We want to run task1 and task2 in parallel
 */
static void test_context_switching()
{
    disable_interrupts();
     
    uint32_t* esp;
    XCREATE_TASK(task1);
    CREATE_TASK(task1, task2);
    CREATE_TASK(task2, task3);
    CREATE_TASK(task3, task4);
    CREATE_TASK(task4, task5);
    CREATE_TASK(task5, task6);
    CREATE_TASK(task6, task7);
    CREATE_TASK(task7, task8);
    CREATE_TASK(task8, task9);
    CREATE_TASK(task9, task10);
    CREATE_TASK(task10, task11);
    CREATE_TASK(task11, task12);
    CREATE_TASK(task12, task13);
    CREATE_TASK(task13, task14);
    CREATE_TASK(task14, task15);
    CREATE_TASK(task15, task16);
    CREATE_TASK(task16, task17);
    CREATE_TASK(task17, task18);
    CREATE_TASK(task18, task19);
    CREATE_TASK(task19, task20);
    task20->next = task1;

    pit_add_timer(schedule_timer, NULL, 50);

    //vga_enable = false;
    switch_task(task1);
}

void kmain(const struct multiboot_info* multiboot, uint32_t multiboot_magic)
{
    vga_init();
    trace_init();
    gdt_init();

    extern void test_format();
    ADD_TEST(test_format);

    /* Processor detection */
    if(is_386()) {
        TRACE("CPU: 80386");
    } else if(is_486()) {
        TRACE("CPU: 80486");
    } else {
        TRACE("CPU: Pentium+");
    }

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
        TRACE("    0x%08lX 0x%08lX 0x%08lX", addr, len, e->type);
    }

    /* setup IDT */
    idt_init();
    idt_add_handler(0x0D, handle_gpf, 3);
    idt_add_handler(0x0E, handle_page_fault, 3);

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
    TRACE("_heap: %p, new_frame: 0x%08lX", _heap, new_frame);
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
    kmalloc_heap = ALIGN_PTR(_heap, VMM_PAGESIZE);
    kmalloc_heap_end = (void*)0x00400000;
    ADD_TEST(kmalloc_test);

    /* Setup PIC */
    pic_init();

    /* Setup PIT */
    pit_init();

    /* Add keyboard handler */
    kbd_init();

    ///* Run user-mode tests */
    //test_usermode();
    //HALT();

    /* Run context switching tests */
    test_context_switching();
    HALT();

#ifdef UNIT_TESTS
    /* Run unit tests */
    run_tests();
    TRACE("All tests done. Kernel Halted.");
    HALT();
#endif

    /* Enable interrupts */
    TRACE("Waiting for an interrupt");
    asm volatile("sti":::"memory");

    /* Wait for an interrupt */
    while(1) {
        asm volatile("hlt":::"memory");
    }
}

