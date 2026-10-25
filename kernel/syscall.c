#include "syscall.h"
#include "bitset.h"
#include "event_queue.h"
#include "exec.h"
#include "idt.h"
#include "kernel.h"
#include "kmalloc.h"
#include "kthread.h"
#include "pit.h"
#include "process.h"
#include "ps2kbd.h"
#include "ramdisk.h"
#include "scancode.h"
#include "tar.h"
#include "trace.h"
#include "vga.h"
#include "vmm.h"
#include <string.h>

static int32_t syscall_getticks(uint32_t* edx)
{
    uint64_t ticks = pit_get_ticks();
    *edx = ticks >> 32;
    return ticks & 0xffffffff;
}

static int32_t syscall_sleep(uint32_t ticks)
{
    thread_sleep(ticks);
    return -1;
}

static void syscall_exit(uint8_t status)
{
    process_exit(status);
}

static void syscall_exit_thread(void)
{
    thread_exit();
}

static int32_t syscall_get_kbd_event(struct KeyEvent* evt, int* overflow,
                                     int block)
{
    if (!vmm_is_valid_userptr(evt, sizeof(*evt), true) ||
        !vmm_is_valid_userptr(overflow, sizeof(*overflow), true)) {
        return -1;
    }

    bool result;
    bool foverflow;
    if (block)
        result = process_get_input_event(evt, &foverflow);
    else
        result = process_peek_input_event(evt, &foverflow);
    *overflow = foverflow;
    return result ? 0 : -1;
}

static int32_t syscall_write(const char* s, size_t len)
{
    if (len > INT32_MAX)
        return -1;
    if (!vmm_is_valid_userptr(s, len, false))
        return -1;
    vga_write(s, len, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    return len;
}

static int32_t syscall_exec(const char* filename, const char* args)
{
    if (!filename)
        return -1;
    if (!vmm_is_valid_userstr(filename))
        return -1;
    if (args && !vmm_is_valid_userstr(args))
        return -1;

    return exec(filename, args);
}

static int32_t syscall_shutdown(void)
{
    debugexit(1);
    outb(0xf4, 1);
    return 0;
}

struct ListContext
{
    void* buffer;
    size_t capacity;
    size_t len;
};

static void list_callback(const struct TarEntry* entry, void* param)
{
    struct ListContext* ctx = param;
    if (entry->is_dir)
        return;

    size_t name_len = strlen(entry->name);
    if (name_len == 0)
        return;
    if (ctx->capacity - ctx->len < name_len + 2)
        return;
    memcpy(ctx->buffer + ctx->len, entry->name, name_len + 1);
    ctx->len += name_len + 1;
}

static int32_t syscall_list(char* buffer, size_t len)
{
    if (!vmm_is_valid_userptr(buffer, len, true))
        return -1;

    struct Tar ramdisk;
    if (!tar_open(&ramdisk, ramdisk_start, ramdisk_end - ramdisk_start))
        return -1;

    struct ListContext ctx = {
        .buffer = buffer,
        .capacity = len,
        .len = 0,
    };
    if (!tar_list(&ramdisk, list_callback, &ctx))
        return -1;

    buffer[ctx.len] = '\0';
    return ctx.len;
}

/*
 * Syscall ABI:
 *  eax: function
 *  ebx: 1st arg
 *  ecx: 2nd arg
 *  edx: 3rd arg
 *  esi: 4th arg
 *  edi: 5th arg
 * Result in eax. Negative values denote errors
 * 64-bit result in eax:edx
 */
void syscall_handler(int, struct isr_regs* regs)
{
    if (thread_current()->process->exit_requested)
        thread_exit();

    switch (regs->eax) {
    case SYSCALL_GETTICKS:
    {
        uint32_t hi;
        regs->eax = syscall_getticks(&hi);
        regs->edx = hi;
        break;
    }
    case SYSCALL_SLEEP:
        regs->eax = syscall_sleep(regs->ebx);
        break;
    case SYSCALL_EXIT:
        syscall_exit(regs->ebx & 0xff);
        panic("Unreachable code");
    case SYSCALL_EXIT_THREAD:
        syscall_exit_thread();
        panic("Unreachable code");
    case SYSCALL_GET_KBD_EVENT:
        regs->eax = syscall_get_kbd_event((struct KeyEvent*)regs->ebx,
                                          (int*)regs->ecx, regs->edx);
        break;
    case SYSCALL_WRITE:
        regs->eax = syscall_write((const char*)regs->ebx, regs->ecx);
        break;
    case SYSCALL_EXEC:
        regs->eax =
            syscall_exec((const char*)regs->ebx, (const char*)regs->ecx);
        break;
    case SYSCALL_SHUTDOWN:
        regs->eax = syscall_shutdown();
        break;
    case SYSCALL_LIST:
        regs->eax = syscall_list((char*)regs->ebx, (size_t)regs->ecx);
        break;
    default:
        regs->eax = (uint32_t)-1;
        break;
    }

    if (thread_current()->process->exit_requested)
        thread_exit();
}

void syscall_init(void)
{
    idt_set_handler(0x80, syscall_handler, 3);

    size_t bitset_count = 65536;
    size_t bitset_len = bitset_storage_size(bitset_count);
    uint32_t* bitset_storage = kmalloc(bitset_len);
    if (!bitset_storage)
        panic("Out of memory");
}

