#include "../user/obj/program1.h"
#include "../user/obj/program2.h"
#include "../user/obj/v86_1.h"
#include "gdt.h"
#include "idt.h"
#include "kbd.h"
#include "kmalloc.h"
#include "logo.h"
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

extern unsigned char _heap_start[];
static unsigned char* _heap = _heap_start;
static struct multiboot_info multiboot_info;
bool early_kmalloc_enabled = true;

#undef V86_DEBUG
extern void v86_enter(struct isr_regs* regs);
extern void v86_return(struct isr_regs*) __attribute__((noreturn));

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

static void handle_ud(struct isr_regs* regs)
{
    PANIC("Invalid opcode at 0x%02lX:0x%08lX", regs->cs, regs->eip);
}

static void handle_gpf(struct isr_regs* regs)
{
#define VALID_FLAGS 0xDFF

#ifdef V86_DEBUG
#define V86_TRACE(...) TRACE(__VA_ARGS__)
#else
#define V86_TRACE(...)
#endif

    if(regs->err_code != 0) {
        TRACE("GPF at 0x%02lX:0x%02lX, error code: 0x%08lX", regs->cs,
              regs->eip, regs->err_code);
    }
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
                V86_TRACE("%p: o32", eip);
                o32 = true;
                eip++;
                regs->eip++;
                break;
            case 0x67: /* a32 */
                V86_TRACE("%p: a32", eip);
                a32 = true;
                eip++;
                regs->eip++;
                break;
            case 0xF0: /* lock */
                V86_TRACE("%p: lock", eip);
                lock = true;
                eip++;
                regs->eip++;
                break;
            case 0xF2: /* repne */
                V86_TRACE("%p: repne", eip);
                repne = true;
                eip++;
                regs->eip++;
                break;
            case 0xF3: /* rep */
                V86_TRACE("%p: rep", eip);
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
                    v86_return(regs);
                } else {
                    V86_TRACE("stack: %p", stack);
                    stack -= 3;
                    stack[0] = (regs->eip + 2) & 0xFFFF;
                    stack[1] = regs->cs & 0xFFFF;
                    stack[2] = regs->eflags & 0xFFFF;
                    regs->cs = ivt[(intnum * 2) + 1];
                    regs->eip = ivt[intnum * 2];
                    regs->esp = ((regs->esp & 0xFFFF) - 6) & 0xFFFF;
                    V86_TRACE("%p: int 0x%02X (0x%02lX:0x%04lX)", eip, intnum,
                              regs->cs, regs->eip);
                    return;
                }
            }
            case 0x9C: /* pushf */
                V86_TRACE("%p: pushf", eip);
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
                V86_TRACE("%p: popf", eip);
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
                V86_TRACE("%p: iret (dest: 0x%02lX:0x%02lX)", eip, regs->cs,
                          regs->eip);
                return;
            case 0xFA: /* cli */
                V86_TRACE("%p: cli", eip);
                regs->eip++;
                return;
            case 0xFB: /* sti */
                V86_TRACE("%p: sti", eip);
                regs->eip++;
                return;
            case 0xE6: /* out imm8, al */
            {
                uint16_t port = eip[1];
                uint8_t val = regs->eax & 0xFF;
                regs->eip += 2;
                V86_TRACE("%p: out 0x%02X, 0x%02X", eip, port, val);
                outb(port, val);
                return;
            }
            case 0xE7: /* out imm8, ax */
            {
                uint16_t port = eip[1];
                if(o32) {
                    uint32_t val = regs->eax;
                    V86_TRACE("%p: out 0x%02X, 0x%08lX", eip, port, val);
                    outl(port, val);
                } else {
                    uint16_t val = regs->eax & 0xFFFF;
                    V86_TRACE("%p: out 0x%02X, 0x%04X", eip, port, val);
                    outw(port, val);
                }
                regs->eip += 2;
                return;
            }
            case 0xEE: /* out dx, al */
            {
                uint16_t port = regs->edx & 0xFFFF;
                uint8_t val = regs->eax & 0xFF;
                V86_TRACE("%p: out 0x%02X, 0x%02X", eip, port, val);
                outb(port, val);
                regs->eip++;
                return;
            }
            case 0xEF: /* out dx, ax */
            {
                uint16_t port = regs->edx & 0xFFFF;
                if(o32) {
                    uint32_t val = regs->eax;
                    V86_TRACE("%p: out 0x%02X, 0x%08lX", eip, port, val);
                    outl(port, val);
                } else {
                    uint16_t val = regs->eax & 0xFFFF;
                    V86_TRACE("%p: out 0x%02X, 0x%04X", eip, port, val);
                    outw(port, val);
                }
                regs->eip++;
                return;
            }
            case 0xEC: /* in al, dx */
            {
                uint16_t port = regs->edx & 0xFFFF;
                V86_TRACE("%p: in al, 0x%02X", eip, port);
                uint8_t val = inb(port);
                regs->eax = (regs->eax & ~0xFF) | val;
                regs->eip++;
                return;
            }
            case 0xED: /* in ax, dx */
            {
                uint16_t port = regs->edx & 0xFFFF;
                if(o32) {
                    V86_TRACE("%p: in eax, 0x%02X", eip, port);
                    uint32_t val = inl(port);
                    regs->eax = val;
                } else {
                    V86_TRACE("%p: in ax, 0x%02X", eip, port);
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

#define VGA_SEQ_INDEX  0x3C4
#define VGA_SEQ_DATA   0x3C5
#define VGA_GC_INDEX   0x3CE
#define VGA_GC_DATA    0x3CF
#define VGA_CRTC_INDEX 0x3D4
#define VGA_CRTC_DATA  0x3D5

/*
    We have two buffers: the frontbuffer and the backbuffer.
    These buffers are in 640x480x8 format, but only the low-order nibble of each pixel is used.
    Draw commands operate on the backbuffer.
    When swap() is called, it calculates the changed pixels and send them to the VGA hardware buffers.
*/
#define FB_WIDTH 640
#define FB_HEIGHT 480
uint8_t* buffers;
uint8_t* frontbuf;
uint8_t* backbuf;

#define OPTIMIZE __attribute__((optimize("03")))

static void setplane(unsigned plane)
{
    static unsigned current = ~0;

    plane &= 3;
    if(plane == current)
        return;
    current = plane;
    unsigned pmask = 1 << plane;
    outw(VGA_GC_INDEX, (plane << 8) | 4);
    outw(VGA_SEQ_INDEX, (pmask << 8) | 2);
}

static void OPTIMIZE putpixel(unsigned x, unsigned y, unsigned color)
{
    if(x >= FB_WIDTH || y >= FB_HEIGHT)
        return;

    backbuf[x + (y * FB_WIDTH)] = color & 0xFF;
}

static void OPTIMIZE fillrect(unsigned x, unsigned y, unsigned w, unsigned h,
                     unsigned col)
{
    uint8_t* ptr;
    uint8_t c = (uint8_t)col;

    if(x + w > FB_WIDTH)
        w = FB_WIDTH - x;
    if(y + h > FB_HEIGHT)
        h = FB_HEIGHT - y;

    for(unsigned j = y; j < y + h; j++) {
        ptr = backbuf + x + (j * FB_WIDTH);
        for(unsigned i = x; i < x + w; i++) {
            *ptr = c;
            ptr++;
        }
    }
}

static void vline(unsigned x, unsigned y, unsigned h, unsigned col)
{
    if(x >= FB_WIDTH)
        return;
    else if(y >= FB_HEIGHT)
        return;
    if(y + h >= FB_HEIGHT)
        h = FB_HEIGHT - y;

    uint8_t* ptr = backbuf + x + (y * FB_WIDTH);
    uint8_t c = (uint8_t)col;
    for(unsigned j = 0; j < h; j++) {
        *ptr = c;
        ptr += FB_WIDTH;
    }
}

static void hline(unsigned x, unsigned y, unsigned w, unsigned col)
{
    if(x >= FB_WIDTH)
        return;
    else if(y >= FB_HEIGHT)
        return;
    if(x + w >= FB_WIDTH)
        w = FB_WIDTH - x;

    uint8_t* ptr = backbuf + x + (y * FB_WIDTH);
    uint8_t c = (uint8_t)col;
    for(unsigned i = 0; i < w; i++) {
        *ptr = c;
        ptr++;
    }
}

static void lineo0(unsigned x0, unsigned y0, unsigned deltax, unsigned deltay,
                   unsigned xdirection, unsigned col)
{
    int deltayx2 = deltay * 2;
    int deltayx2minusdeltaxx2 = deltayx2 - (int)(deltax * 2);
    int errorterm = deltayx2 - (int)deltax;

    putpixel(x0, y0, col);
    while(deltax--) {
        if(errorterm >= 0) {
            y0++;
            errorterm += deltayx2minusdeltaxx2;
        } else {
            errorterm += deltayx2;
        }
        x0 += xdirection;
        putpixel(x0, y0, col);
    }
}

static void lineo1(unsigned x0, unsigned y0, unsigned deltax, unsigned deltay,
                   unsigned xdirection, unsigned col)
{
    int deltaxx2 = deltax * 2;
    int deltaxx2minusdeltayx2 = deltaxx2 - (int)(deltay * 2);
    int errorterm = deltaxx2 - (int)deltay;

    putpixel(x0, y0, col);
    while(deltay--) {
        if(errorterm >= 0) {
            x0 += xdirection;
            errorterm += deltaxx2minusdeltayx2;
        } else {
            errorterm += deltaxx2;
        }
        y0++;
        putpixel(x0, y0, col);
    }
}

static void line(int x0, int y0, int x1, int y1, unsigned color)
{
    if(x0 == x1) {
        if(y0 < y1)
            vline(x0, y0, y1 - y0, color);
        else
            vline(x0, y1, y0 - y1, color);
        return;
    } else if(y0 == y1) {
        if(x0 < x1)
            hline(x0, y0, x1 - x0, color);
        else
            hline(x1, y0, x0 - x1, color);
        return;
    }

    if(y0 > y1) {
        int temp = y0;
        y0 = y1;
        y1 = temp;

        temp = x0;
        x0 = x1;
        x1 = temp;
    }

    int deltax = x1 - x0;
    int deltay = y1 - y0;
    if(deltax > 0) {
        if(deltax > deltay) {
            lineo0(x0, y0, deltax, deltay, 1, color);
        } else {
            lineo1(x0, y0, deltax, deltay, 1, color);
        }
    } else {
        deltax = -deltax;
        if(deltax > deltay) {
            lineo0(x0, y0, deltax, deltay, -1, color);
        } else {
            lineo1(x0, y0, deltax, deltay, -1, color);
        }
    }
}

static void rect(unsigned x, unsigned y, unsigned w, unsigned h, unsigned col)
{
    hline(x, y, w, col);
    hline(x, y + h - 1, w, col);
    vline(x, y, h, col);
    vline(x + w - 1, y, h, col);
}

static void circle(int cx, int cy, int radius, unsigned color)
{
    int x = 0;
    int y = radius;
    int m = 5 - 4 * radius;
    while(x <= y) {
        putpixel(cx + x, cy + y, color);
        putpixel(cx + x, cy - y, color);
        putpixel(cx - x, cy + y, color);
        putpixel(cx - x, cy - y, color);
        putpixel(cx + y, cy + x, color);
        putpixel(cx + y, cy - x, color);
        putpixel(cx - y, cy + x, color);
        putpixel(cx - y, cy - x, color);
        if(m > 0) {
            y--;
            m -= 8 * y;
        }
        x++;
        m += 8 * x + 4;
    }
}

void fillcircle(int centerX, int centerY, int radius, unsigned c)
{
    int x = 0;
    int y = radius;
    int m = 5 - 4 * radius;

    while(x <= y) {
        hline(centerX - y, centerY - x, y * 2, c);
        hline(centerX - y, centerY + x, y * 2, c);

        if(m > 0) {
            hline(centerX - x, centerY - y, x * 2, c);
            hline(centerX - x, centerY + y, x * 2, c);
            y--;
            m -= 8 * y;
        }

        x++;
        m += 8 * x + 4;
    }
}

static void int10(struct isr_regs* regs)
{
    /*
     * Identity-map the first 64k, and from the EBDA to 1MB
     * This will give us more than 64k or stack
     *  - 0x00000000 - 0x0000FFFF
     *  - 0x00080000 - 0x000FFFFF
     */
    for(const uint8_t* ptr = (uint8_t*)0x0; ptr < (uint8_t*)0xFFFF;
        ptr += VMM_PAGESIZE) {
        bool ret =
            vmm_map(ptr, (uintptr_t)ptr, VMM_PRESENT | VMM_USER | VMM_WRITABLE);
        if(!ret) {
            PANIC("vmm_map failed");
        }
    }

    for(const uint8_t* ptr = (uint8_t*)0x80000; ptr < (uint8_t*)0x100000;
        ptr += VMM_PAGESIZE) {
        if(ptr < (uint8_t*)VGA_START || ptr > (uint8_t*)VGA_END) {
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
    uint8_t* stub = (uint8_t*)0x500;
    memset(stub, 0xCC, 0x10000 - (uintptr_t)stub);

    uint8_t* ptr = stub;
    *ptr++ = 0xCD; /* int */
    *ptr++ = 0x10; /* 0x10 */

    *ptr++ = 0xCD; /* int */
    *ptr++ = 0x80; /* 0x80 */
    *ptr++ = 0xCC; /* int3 */

    /* iret with VM flags set */
    CLEAR_IF();
    iomap_allow_all();
    regs->esp = 0x10000;
    regs->eip = (uint32_t)stub;
    regs->eflags = (read_eflags() | EFLAGS_VM) & ~EFLAGS_IF & ~EFLAGS_IOPL;
    v86_enter(regs);
    iomap_deny_all();
    RESTORE_IF();

    /* Cleanup */
    for(const uint8_t* ptr = (uint8_t*)0x0; ptr < (uint8_t*)0xFFFF;
        ptr += VMM_PAGESIZE) {
        bool ret = vmm_unmap(ptr, false);
        if(!ret) {
            PANIC("vmm_unmap failed");
        }
    }

    for(const uint8_t* ptr = (uint8_t*)0x80000; ptr < (uint8_t*)0x100000;
        ptr += VMM_PAGESIZE) {
        if(ptr < (uint8_t*)VGA_START || ptr > (uint8_t*)VGA_END) {
            bool ret = vmm_unmap(ptr, false);
            if(!ret) {
                PANIC("vmm_unmap failed");
            }
        }
    }
}


static void drawstring(const uint8_t* vga_font, unsigned x, unsigned y,
                       unsigned col, const char* str, size_t len)
{
    if(len == -1)
        len = strlen(str);

    int currx = x;
    int curry = y;
    while(len) {
        if(*str >= ' ') {
            const uint8_t* glyph = vga_font + (*str * 16);
            for(unsigned scanline = 0; scanline < 16; scanline++) {
                for(int bit = 7; bit >= 0; bit--) {
                    if(*glyph & (1 << bit)) {
                        putpixel(currx, curry, col);
                    }
                    currx++;
                }
                glyph++;
                currx = x;
                curry++;
            }
        }
        x += 8;
        currx = x;
        curry = y;
        str++;
        len--;
    }
}

static  void clearscreen(unsigned color)
{
    memset(backbuf, color & 0xF, FB_WIDTH * FB_HEIGHT);
}

static void OPTIMIZE swapbuffers()
{
#if 1
    for(unsigned plane = 0; plane < 4; plane++) {
        setplane(plane);

        uint8_t* backptr;
        uint8_t* frontptr;
        uint8_t* vgaptr;
        for(backptr = backbuf, frontptr = frontbuf, vgaptr = (uint8_t*)VGA_START;
            backptr < backbuf + (FB_WIDTH * FB_HEIGHT);
            backptr += 8, frontptr += 8, vgaptr++
        ) {
            bool dirty = false;
            uint8_t value = 0;
            for(unsigned i = 0; i < 8; i++) {
                if(backptr[i] != frontptr[i]) {
                    dirty = true;
                }
                if(backptr[i] & (1 << plane)) {
                    value |= 1 << (7 - i);
                }
            }
            if(dirty) {
                *vgaptr = value;
            }
        }
    }
#else
    unsigned pmask = 1;
    uint8_t* vga_base = (uint8_t*)VGA_START;
    unsigned width_bytes = 640 / 8;

    for(unsigned plane = 0; plane < 4; plane++) {
        setplane(plane);

        uint8_t* back = backbuf;
        uint8_t* front = frontbuf;
        for(unsigned y = 0; y < FB_HEIGHT; y++) {
            uint8_t* ptr = vga_base + (y * width_bytes);
            for(unsigned x = 0; x < FB_WIDTH; x++) {
                if(*back != *front) {
                    unsigned color = *back & 0xF;
                    unsigned mask = 0x80 >> (x & 7); /* 0b0111 */
                    if(color & pmask) {
                        ptr[x/8] |= mask;
                    } else {
                        ptr[x/8] &= ~mask;
                    }
                }
                back++;
                front++;
            }
        }

        pmask <<= 1;
    }
#endif
    if(frontbuf == buffers) {
        backbuf = buffers;
        frontbuf = buffers + (FB_WIDTH * FB_HEIGHT);
    } else {
        frontbuf = buffers;
        backbuf = buffers + (FB_WIDTH * FB_HEIGHT);
    }
}

static void v86_test()
{
    vga_enable = false;

    /* Map VGA display memory */
    for(const uint8_t* p = (uint8_t*)VGA_START; p < (uint8_t*)VGA_END;
        p += VMM_PAGESIZE) {
        if(p != (uint8_t*)VGA_BASE) {
            if(!vmm_map(p, (uintptr_t)p, VMM_PRESENT | VMM_USER | VMM_WRITABLE | VMM_PCD))
                PANIC("vmm_map failed");
        }
    }

    /* Initialize buffers */
#if 1
    size_t buffers_size = (FB_WIDTH * FB_HEIGHT * 2) + ((FB_WIDTH * FB_HEIGHT * 4) / 8);
    TRACE("buffers_size: %zu", buffers_size);
    buffers = kmalloc(buffers_size);
    memset(buffers, 0, buffers_size);
    backbuf = buffers;
    frontbuf = buffers + (FB_WIDTH * FB_HEIGHT);
#else
    buffers = kmalloc(FB_WIDTH * FB_HEIGHT * 2);
    memset(buffers, 0, FB_WIDTH * FB_HEIGHT * 2);
    backbuf = buffers;
    frontbuf = buffers + (FB_WIDTH * FB_HEIGHT);
#endif

    /* get bios 8x16 font */
    struct isr_regs regs;
    memset(&regs, 0, sizeof(regs));
    regs.eax = 0x1130;
    regs.ebx = 0x0600;
    int10(&regs); /* vgafont in es:bp */
    TRACE("vga font in 0x%02lX:%04lX", regs.v86_es, regs.ebp);

    // uint8_t* vga_font = kmalloc(8192);
    const uint8_t* vga_font_ptr =
        (uint8_t*)((regs.v86_es * 0x10) + (regs.ebp & 0xFFFF));
    const uint8_t* vga_font_frame = ROUND_PTR(vga_font_ptr, VMM_PAGESIZE);
    if(!vmm_map_range(vga_font_frame, (uintptr_t)vga_font_frame,
                      8192 + VMM_PAGESIZE, VMM_PRESENT))
        PANIC("vmm_map_range failed");

    uint8_t* vga_font = kmalloc(8192);
    memcpy(vga_font, vga_font_ptr, 8192);

    if(!vmm_unmap_range(vga_font_frame, 8192 + VMM_PAGESIZE, false))
        PANIC("vmm_unmap_range failed");

    /* switch to vga mode 12 */
    memset(&regs, 0, sizeof(regs));
    regs.eax = 0x0012;
    int10(&regs);

    int curx = FB_WIDTH / 2;
    int cury = FB_HEIGHT / 2;
    bool dirty = true;
    enable_interrupts();
    while(1) {
        struct kbd_event evt;
        if(kbd_read(&evt)) {
            if(evt.type == KBD_EVENT_PRESSED) {
                switch(evt.scancode) {
                case 0xC8: /* up */
                    if(cury > 5)
                        cury -= 5;
                    break;
                case 0xD0: /* down */
                    if(cury < FB_HEIGHT - 5)
                        cury += 5;
                    break;
                case 0xCB: /* left */
                    if(curx > 5)
                        curx -= 5;
                    break;
                case 0xCD: /* right */
                    if(curx < FB_WIDTH - 5)
                        curx += 5;
                    break;
                case 0x1: /* esc */
                    curx = FB_WIDTH / 2;
                    cury = FB_HEIGHT / 2;
                    break;
                default:
                    TRACE("pressed: 0x%X", evt.scancode);
                }
                dirty = true;
            }
        }

        if(dirty) {
            uint64_t startts = RDTSC();

            clearscreen(0);
            putpixel(curx, cury, 2);
            
            for(unsigned col = 0; col < 16; col++) {
                unsigned startx = curx + 5 + ((col % 8) * 32);
                unsigned starty = cury + 5 + ((col / 8) * 32);
                fillrect(startx, starty, 32, 32, col);
            }

            unsigned index = 0;
            for(unsigned y = cury; y < cury + logo_height; y++) {
                for(unsigned x = curx; x < curx + logo_width; x++) {
                    putpixel(x, y, logo_data[index++]);
                }
            }

            rect(curx, cury, 300, 72, 10);

            line(32, 32, curx, cury, 11);
            line(curx + 300, cury + 72, FB_WIDTH - 32, FB_HEIGHT - 32, 3);
            line(curx, cury + 72, 32, FB_HEIGHT - 32, 2);
            line(curx + 300, cury, FB_WIDTH - 32, 32, 6);

            circle(640 / 2, 480 / 2, 128, 12);
            fillcircle(curx + 300, cury + 128, 16, 14);

            drawstring(vga_font, curx, cury - 32, 9, "All your base are belong to us", -1);

            swapbuffers();
            dirty = false;

            uint64_t endts = RDTSC();
            uint64_t timing = (endts - startts) >> 14;
            assert(timing < UINT32_MAX);
            TRACE("Draw finished in %u cycles", (unsigned)timing);
        }
        HLT();
    }
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
    idt_add_handler(0x06, handle_ud, 3);
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
