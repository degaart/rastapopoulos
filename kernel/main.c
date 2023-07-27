#include "../user/obj/program1.h"
#include "../user/obj/program2.h"
#include "gdt.h"
#include "idt.h"
#include "kbd.h"
#include "kmalloc.h"
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

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#define ENTER_USERMODE(esp, eip)                \
    asm volatile("cli\n"                        \
                 "mov   ax, 0x23\n"             \
                 "mov   ds, ax\n"               \
                 "mov   es, ax\n"               \
                 "mov   fs, ax\n"               \
                 "mov   gs, ax\n"               \
                 "pushd 0x23\n"                 \
                 "pushd ebx\n"                  \
                 "pushf\n"                      \
                 "pop   eax\n"                  \
                 "or    eax, 0x200\n"           \
                 "pushd eax\n"                  \
                 "pushd 0x18|0x3\n"             \
                 "push  ecx\n"                  \
                 "iretd\n"                      \
                 :: "ebx"(esp), "ecx"(eip))

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
    CLEAR_IF();
    uint32_t cr2 = read_cr2();
    TRACE("Page fault for 0x%08lX", cr2);
    TRACE("CS: 0x%lX, EIP: 0x%08lX, ESP: 0x%08lX", regs->cs, regs->eip,
          regs->esp);

#define PF_P (1 << 0)
#define PF_WR (1 << 1)
#define PF_US (1 << 2)
#define PF_RSVD (1 << 3)
#define PF_ID (1 << 4)
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
    CLEAR_IF();
    char prefix[64];
    snprintf(prefix, sizeof(prefix), "[%s:%s:%d] ", basename(file), fn, line);
    serial_write_string(prefix);
    vga_write_string(prefix, COLOR_LIGHTGRAY);

    va_list args;
    va_start(args, fmt);
    formatv(debug_write, NULL, fmt, args);
    va_end(args);

    debug_write('\n', NULL);
    RESTORE_IF();
}

void panic(const char* file, int line, const char* fn, const char* fmt, ...)
{
    CLEAR_IF();
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
    TRACE("kmalloc_heap: %p, kmalloc_heap_end: %p", kmalloc_heap,
          kmalloc_heap_end);

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
    for(int i = 0; i < sizeof(ptrs) / sizeof(ptrs[0]); i++) {
        size_t size = (1 << i);
        TRACE("Allocating 0x%lX bytes", size);
        ptrs[i] = kmalloc(size);
    }

    for(int i = (sizeof(ptrs) / sizeof(ptrs[0])) - 1; i >= 0; i--) {
        kfree(ptrs[i]);
    }
    kmalloc_trim(0);
    TRACE("footprint: 0x%lX, kmalloc_heap: %p", kmalloc_footprint(),
          kmalloc_heap);
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
    // TRACE("Syscall handler called");
    // TRACE("esp: 0x%08lX", read_esp());
    // TRACE("IF: %s", interrupts_enabled() ? "SET" : "CLEAR");
    switch(regs->eax) {
    case 0: /* trace */
        syscall0(regs);
        break;
    case 1: /* add ebx+ecx+edx */
        syscall1(regs);
        break;
    case 2: /* panic */
        syscall2(regs);
        break;
    case 3: /* halt */
        syscall3(regs);
        break;
    case 4: /* getticks */
        syscall4(regs);
        break;
    default:
        PANIC("Invalid syscall 0x%02lX", regs->eax);
        break;
    }
}

struct task {
    /*
     * `esp` must be the first field in this structure because switch_task
     * does not have a complete definition of `struct task`
     */
    uint32_t esp;
    struct pagedir* pagedir;
    void* stack;
    char* name;
    struct task* next;
};

struct task* ready_queue = NULL;
struct task* current_task = NULL;

void switch_task(struct task* task)
{
    void switch_task_impl(struct task*);

    CLEAR_IF();
    vmm_set_pagedir(task->pagedir);
    tss_set_esp0(task->stack + VMM_PAGESIZE);
    switch_task_impl(task);
    RESTORE_IF();
}

static void schedule_timer(uint64_t ticks, void* ctx)
{
    switch_task(current_task->next);
}

char* strdup(const char* str)
{
    size_t len = strlen(str);
    char* buffer = kmalloc(len + 1);
    memcpy(buffer, str, len);
    return buffer;
}

struct task* task_create(const char* name, void (*entry)(void))
{
    struct task* task = kmalloc(sizeof(struct task));
    memset(task, 0, sizeof(struct task));
    task->stack = kpvalloc(VMM_PAGESIZE);
    uint32_t* esp = (uint32_t*)(task->stack + VMM_PAGESIZE);
    *(--esp) = 0;
    *(--esp) = (uintptr_t)entry;
    *(--esp) = 0xDEADBEE0;
    *(--esp) = 0xDEADBEE1;
    *(--esp) = 0xDEADBEE2;
    *(--esp) = 0xDEADBEE3;
    task->esp = (uintptr_t)esp;
    task->pagedir = vmm_create_pagedir();
    task->name = strdup(name);
    if(ready_queue) {
        struct task* last = ready_queue;
        for(; last->next != last && last->next != ready_queue; last = last->next)
            ;
        if(last->next == last) {
            task->next = last;
            last->next = task;
        } else {
            task->next = ready_queue;
            last->next = task;
        }
        ready_queue = task;
    } else {
        ready_queue = task;
        task->next = task;
    }

    return task;
}

static void counter1_entry()
{
    enable_interrupts();

    /* Map program starting at 0x400000 */
    unsigned char* dst;
    const unsigned char* src;
    for(dst = (unsigned char*)0x400000, src = obj_program1_elf;
        src < obj_program1_elf + obj_program1_elf_len;
        dst += VMM_PAGESIZE, src += VMM_PAGESIZE) {
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
    tss_set_esp0(kernelstack + VMM_PAGESIZE);

    void user_entry(void);
    uint32_t esp = (uintptr_t)userstack + VMM_PAGESIZE;
    uint32_t eip = 0x00400000;
    ENTER_USERMODE(esp, eip);
    INVALID_CODE_PATH();
}

static void counter2_entry()
{
    enable_interrupts();

    /* Map program starting at 0x400000 */
    unsigned char* dst;
    const unsigned char* src;
    for(dst = (unsigned char*)0x400000, src = obj_program2_elf;
        src < obj_program2_elf + obj_program2_elf_len;
        dst += VMM_PAGESIZE, src += VMM_PAGESIZE) {
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
    tss_set_esp0(kernelstack + VMM_PAGESIZE);

    void user_entry(void);
    uint32_t esp = (uintptr_t)userstack + VMM_PAGESIZE;
    uint32_t eip = 0x00400000;
    ENTER_USERMODE(esp, eip);
    INVALID_CODE_PATH();
}

static void yield()
{
    switch_task(current_task->next);
}

/*
 * first row of display: scheduler information
 * second row: first userspace process (counter)
 * third row: second userspace process (counter)
 */
static void kernel_task_entry()
{
    /* Every newly-created task starts with IF clear */
    enable_interrupts();
    struct task* counter1 = task_create("counter1", counter1_entry);
    struct task* counter2 = task_create("counter2", counter2_entry);
    while(1) {
        yield();
    }
}

void kmain(const struct multiboot_info* multiboot, uint32_t multiboot_magic)
{
    vga_init();
    trace_init();
    gdt_init();

#ifdef UNIT_TESTS
    extern void test_format();
    ADD_TEST(test_format);
#endif

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
        struct multiboot_mmap_entry* mmap_entries =
            early_kmalloc(multiboot_info.mmap_length);
        memcpy(mmap_entries, multiboot_info.mmap_addr,
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
    MULTIBOOT_MMAP_ITERATE(multiboot_info.mmap_addr, e,
                           multiboot_info.mmap_length)
    {
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
        (uintptr_t)e <
        (uintptr_t)multiboot_info.mmap_addr + multiboot_info.mmap_length;
        e = (const struct multiboot_mmap_entry*)((uintptr_t)e + e->size +
                                                 sizeof(uint32_t))) {
        if(e->type == MULTIBOOT_MEMORY_AVAILABLE) {
            uint32_t start = ALIGN(e->addr & 0xFFFFFFFF, VMM_PAGESIZE);
            uint32_t size =
                ROUND((e->len & 0xFFFFFFFF) - (start - (e->addr & 0xFFFFFFFF)),
                      VMM_PAGESIZE);
            for(uint32_t frame = start; frame < start + size;
                frame += VMM_PAGESIZE) {
                if(frame <= (uint32_t)_heap) { /* 1Mb */
                    pmm_set(frame);
                }
            }
        }
    }

    /* At this point, if we pmm_alloc(), we should get a page which is > _heap
     */
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

    // /* Run user-mode tests */
    // test_usermode();
    // HALT();

    // /* Run context switching tests */
    // test_context_switching();
    // HALT();

#ifdef UNIT_TESTS
    /* Run unit tests */
    run_tests();
    TRACE("All tests done. Kernel Halted.");
    HALT();
#endif

    /* register system call handler */
    idt_add_handler(0x30, syscall_handler, 3);

    /* Create kernel_task and execute it */
    struct task* task = task_create("kernel_task", kernel_task_entry);
    pit_add_timer(schedule_timer, NULL, 1);
    vga_enable = false;
    switch_task(task);
    INVALID_CODE_PATH();
}
