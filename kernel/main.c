#include "../user/obj/program1.h"
#include "../user/obj/program2.h"
#include "../user/obj/v86_1.h"
#include "gdt.h"
#include "idt.h"
#include "kbd.h"
#include "kmalloc.h"
#include "pic.h"
#include "pit.h"
#include "pmm.h"
#include "vmm.h"
#include <debug.h>
#include <io.h>
#include <multiboot.h>
#include <rbuf.h>
#include <serial.h>
#include <string.h>
#include <syscall.h>
#include <util.h>
#include <vga.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#define ENTER_USERMODE(esp, eip)                                               \
    asm volatile("cli\n"                                                       \
                 "mov   ax, 0x23\n"                                            \
                 "mov   ds, ax\n"                                              \
                 "mov   es, ax\n"                                              \
                 "mov   fs, ax\n"                                              \
                 "mov   gs, ax\n"                                              \
                 "pushd 0x23\n"                                                \
                 "pushd ebx\n"                                                 \
                 "pushf\n"                                                     \
                 "pop   eax\n"                                                 \
                 "or    eax, 0x200\n"                                          \
                 "pushd eax\n"                                                 \
                 "pushd 0x18|0x3\n"                                            \
                 "push  ecx\n"                                                 \
                 "iretd\n" ::"ebx"(esp),                                       \
                 "ecx"(eip))

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

#define PF_P    (1 << 0)
#define PF_WR   (1 << 1)
#define PF_US   (1 << 2)
#define PF_RSVD (1 << 3)
#define PF_ID   (1 << 4)
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
#define VALID_FLAGS 0xDFF
    // TRACE("General protection fault at 0x%lX:0x%08lX", regs->cs, regs->eip);
    // TRACE("Error code: 0x%lX", regs->err_code);
    // TRACE("EFLAGS: 0x%lX", regs->eflags);
    assert(regs->err_code == 0);
    if(regs->eflags & EFLAGS_VM) {
        bool o32 = false;
        bool a32 = false;
        bool rep = false;
        bool repne = false;
        bool lock = false;
        uint16_t* ivt = (uint16_t*)0x00000000; /* Undefined behaviour */
        uint16_t* stack = (uint16_t*)((regs->ss * 0x10) + regs->esp);
        uint8_t* eip = (uint8_t*)((regs->cs * 0x10) + regs->eip);
        while(1) {
            switch(*eip) {
            case 0x66: /* o32 */
                TRACE("%p: o32", eip);
                o32 = true;
                eip++;
                regs->eip++;
                break;
            case 0x67: /* a32 */
                TRACE("%p: a32", eip);
                a32 = true;
                eip++;
                regs->eip++;
                break;
            case 0xF0: /* lock */
                TRACE("%p: lock", eip);
                lock = true;
                eip++;
                regs->eip++;
                break;
            case 0xF2: /* repne */
                TRACE("%p: repne", eip);
                repne = true;
                eip++;
                regs->eip++;
                break;
            case 0xF3: /* rep */
                TRACE("%p: rep", eip);
                rep = true;
                eip++;
                regs->eip++;
                break;
            case 0x2E: /* cs prefix */
            case 0x36: /* ss prefix */
            case 0x3E: /* ds prefix */
            case 0x26: /* es prefix */
            case 0x64: /* fs prefix */
            case 0x65: /* gs prefix */
                PANIC("Unsupported segment override prefix");
                break;
            case 0xCD: /* int */
            {
                int intnum = eip[1];
                if(intnum == 0x80) { /* vm exit */
                    extern void v86_return(void) __attribute__((noreturn));
                    v86_return();
                } else {
                    stack -= 3;
                    stack[0] = (regs->eip + 2) & 0xFFFF;
                    stack[1] = regs->cs & 0xFFFF;
                    stack[2] = regs->eflags & 0xFFFF;
                    regs->cs = ivt[(intnum * 2) + 1];
                    regs->eip = ivt[intnum * 2];
                    regs->esp = ((regs->esp & 0xFFFF) - 6) & 0xFFFF;
                    TRACE("%p: int 0x%02X (0x%02lX:0x%04lX)", eip, intnum,
                          regs->cs, regs->eip);
                    return;
                }
            }
            case 0x9C: /* pushf */
                TRACE("%p: pushf", eip);
                if(o32) {
                    regs->esp = ((regs->esp & 0xFFFF) - 4) & 0xFFFF;
                    unsigned flags = regs->eflags & VALID_FLAGS;
                    stack -= 2;
                    stack[0] = flags & 0xFFFF;
                    stack[1] = (flags & 0xFFFF0000) >> 16;
                } else {
                    regs->esp = ((regs->esp & 0xFFFF) - 2) & 0xFFFF;
                    stack--;
                    stack[0] = regs->eflags & VALID_FLAGS;
                }
                regs->eip++;
                return;
            case 0x9D: /* popf */
                TRACE("%p: popf", eip);
                if(o32) {
                    unsigned flags =
                        (stack[0] | (stack[1] << 16)) & VALID_FLAGS;
                    regs->eflags = EFLAGS_VM | flags;
                    regs->esp = ((regs->esp & 0xFFFF) + 4) & 0xFFFF;
                } else {
                    regs->eflags = EFLAGS_VM | (stack[0] & VALID_FLAGS);
                    regs->esp = ((regs->esp & 0xFFFF) + 2) & 0xFFFF;
                }
                regs->eip++;
                return;
            case 0xCF: /* iret */
                assert(!o32);
                regs->eip = stack[0];
                regs->cs = stack[1];
                regs->eflags = EFLAGS_IF | EFLAGS_VM | stack[2];
                regs->esp = ((regs->esp & 0xFFFF) + 6) & 0xFFFF;
                TRACE("%p: iret (dest: 0x%02lX:0x%02lX)", eip, regs->cs,
                      regs->eip);
                return;
            case 0xFA: /* cli */
                TRACE("%p: cli", eip);
                regs->eip++;
                return;
            case 0xFB: /* sti */
                TRACE("%p: sti", eip);
                regs->eip++;
                return;
            case 0xE6: /* out imm8, al */
            {
                uint16_t port = eip[1];
                uint8_t val = regs->eax & 0xFF;
                regs->eip += 2;
                TRACE("%p: out 0x%02X, 0x%02X", eip, port, val);
                outb(port, val);
                return;
            }
            case 0xE7: /* out imm8, ax */
            {
                uint16_t port = eip[1];
                if(o32) {
                    uint32_t val = regs->eax;
                    TRACE("%p: out 0x%02X, 0x%08lX", eip, port, val);
                    outl(port, val);
                } else {
                    uint16_t val = regs->eax & 0xFFFF;
                    TRACE("%p: out 0x%02X, 0x%04X", eip, port, val);
                    outw(port, val);
                }
                regs->eip += 2;
                return;
            }
            case 0xEE: /* out dx, al */
            {
                uint16_t port = regs->edx & 0xFFFF;
                uint8_t val = regs->eax & 0xFF;
                TRACE("%p: out 0x%02X, 0x%02X", eip, port, val);
                outb(port, val);
                regs->eip++;
                return;
            }
            case 0xEF: /* out dx, ax */
            {
                uint16_t port = regs->edx & 0xFFFF;
                if(o32) {
                    uint32_t val = regs->eax;
                    TRACE("%p: out 0x%02X, 0x%08lX", eip, port, val);
                    outl(port, val);
                } else {
                    uint16_t val = regs->eax & 0xFFFF;
                    TRACE("%p: out 0x%02X, 0x%04X", eip, port, val);
                    outw(port, val);
                }
                regs->eip++;
                return;
            }
            case 0xEC: /* in al, dx */
            {
                uint16_t port = regs->edx & 0xFFFF;
                TRACE("%p: in al, 0x%02X", eip, port);
                uint8_t val = inb(port);
                regs->eax = (regs->eax & ~0xFF) | val;
                regs->eip++;
                return;
            }
            case 0xED: /* in ax, dx */
            {
                uint16_t port = regs->edx & 0xFFFF;
                if(o32) {
                    TRACE("%p: in eax, 0x%02X", eip, port);
                    uint32_t val = inl(port);
                    regs->eax = val;
                } else {
                    TRACE("%p: in ax, 0x%02X", eip, port);
                    uint16_t val = inw(port);
                    regs->eax = (regs->eax & ~0xFFFF) | val;
                }
                regs->eip++;
                return;
            }
            default:
                PANIC("Unhandled instruction: 0x%02X", *eip);
                break;
            }
        }
    }
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

static void syscall_trace(struct isr_regs* regs)
{
    TRACE("%s", (const char*)regs->ebx);
}

static void syscall_add(struct isr_regs* regs)
{
    regs->eax = regs->ebx + regs->ecx + regs->edx;
}

static void syscall_panic(struct isr_regs* regs)
{
    PANIC("Panic from usermode");
}

static void syscall_halt(struct isr_regs* regs)
{
    TRACE("Halt from usermode");
    HALT();
}

static void syscall_getticks(struct isr_regs* regs)
{
    uint64_t ticks = get_ticks();
    regs->eax = ticks & 0xFFFFFFFF;
    regs->ebx = ticks >> 32;
}

static void syscall_readkbd(struct isr_regs* regs)
{
}

static void syscall_handler(struct isr_regs* regs)
{
    // TRACE("Syscall handler called");
    // TRACE("esp: 0x%08lX", read_esp());
    // TRACE("IF: %s", interrupts_enabled() ? "SET" : "CLEAR");
    switch(regs->eax) {
    case SYSCALL_TRACE: /* trace */
        syscall_trace(regs);
        break;
    case SYSCALL_ADD: /* add ebx+ecx+edx */
        syscall_add(regs);
        break;
    case SYSCALL_PANIC: /* panic */
        syscall_panic(regs);
        break;
    case SYSCALL_HALT: /* halt */
        syscall_halt(regs);
        break;
    case SYSCALL_GETTICKS: /* getticks */
        syscall_getticks(regs);
        break;
    case SYSCALL_READKBD:
        syscall_readkbd(regs);
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
        for(; last->next != last && last->next != ready_queue;
            last = last->next)
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

    /* init tss */
    uint8_t* kernel_stack = kpvalloc(VMM_PAGESIZE);
    tss_set_esp0(kernel_stack + VMM_PAGESIZE);

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
 * commandline shell as usermode
 *  - keyboard reader
 *  - vga interface
 */
static void kernel_task_entry()
{
    /* Every newly-created task starts with IF clear */
    enable_interrupts();
    struct task* counter1 = task_create("counter1", counter1_entry);
    struct task* counter2 = task_create("counter2", counter2_entry);

    /* ring buffer test */
    test_rbuf();

    /* idle loop */
    uint16_t* vga_base = (uint16_t*)VGA_BASE;
    char msgbuf[VGA_WIDTH + 1];
    while(1) {
        struct kbd_event evt;
        if(kbd_read(&evt)) {
            snprintf(msgbuf, sizeof(msgbuf),
                     "%10s 0x%02X %c %4s %3s %5s %5s %5s",
                     evt.type == KBD_EVENT_PRESSED ? "pressed" : "released",
                     evt.scancode, evt.ch ? evt.ch : ' ',
                     (evt.modifiers & KBD_MOD_CTRL) ? "ctrl" : "",
                     (evt.modifiers & KBD_MOD_ALT) ? "alt" : "",
                     (evt.modifiers & KBD_MOD_ALTGR) ? "altgr" : "",
                     (evt.modifiers & KBD_MOD_SHIFT) ? "shift" : "",
                     (evt.modifiers & KBD_MOD_SUPER) ? "super" : "");
            uint16_t* d = vga_base;
            for(const char* p = msgbuf; *p; p++, d++) {
                *d = *p | ((COLOR_YELLOW | (COLOR_BLUE << 4)) << 8);
            }
        }
        yield();
    }
}

#ifdef V86_TEST
static void v86_test()
{
    /* Identity-map first mega */
    for(const uint8_t* ptr = 0; ptr < (const uint8_t*)0x100000;
        ptr += VMM_PAGESIZE) {
        if(ptr != (const uint8_t*)VGA_BASE) {
            bool ret = vmm_map(ptr, (uintptr_t)ptr,
                               VMM_PRESENT | VMM_USER | VMM_WRITABLE);
            if(!ret) {
                PANIC("vmm_map failed");
            }
        }
    }

    /*
     * Conventional memory: 0x00000500 - 0x0007FFFF
     */
    const char* message = "All your base are belong to us\r\n";
    uint8_t* stub = (uint8_t*)0x500;
    uint8_t* ptr = stub;
    *ptr++ = 0x87; /* xchg */
    *ptr++ = 0xDB; /* bx, bx */
    *ptr++ = 0xCD; /* int */
    *ptr++ = 0x10; /* 0x10 */
    *ptr++ = 0xCD; /* int */
    *ptr++ = 0x80; /* 0x80 */
    *ptr++ = 0xCC; /* int3 */
    memcpy(ptr, message, strlen(message) + 1);

    /* We're toying with the screen, so disable VGA */
    vga_enable = false;

    /* iret with VM flags set */
    void v86_enter(struct isr_regs * regs);
    TRACE("Entering v86");

    struct isr_regs regs;
    memset(&regs, 0, sizeof(regs));
    regs.eax = 0x1301;
    regs.ebx = 0x000F;
    regs.ecx = strlen(message);
    regs.ebp = (uintptr_t)ptr;
    regs.eip = (uintptr_t)stub;
    regs.esp = 0xFFFF;
    regs.eflags = (read_eflags() | EFLAGS_VM) & ~EFLAGS_IF & ~EFLAGS_IOPL;
    v86_enter(&regs);
    TRACE("V86 call done");
    HALT();
}
#endif

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

#ifdef V86_TEST
    v86_test();
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
