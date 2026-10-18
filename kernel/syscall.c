#include "syscall.h"
#include "bitset.h"
#include "idt.h"
#include "kernel.h"
#include "kmalloc.h"
#include "kthread.h"
#include "pit.h"
#include "process.h"
#include "ps2kbd.h"
#include "trace.h"

static struct Bitset kbd_bitset;

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

static int32_t syscall_keydown(uint32_t scancode)
{
    if (scancode > 0xffff)
        return -1;
    bool ret = bitset_test(&kbd_bitset, scancode);
    return ret ? 1 : 0;
}

static void syscall_exit(uint8_t status)
{
    process_exit(status);
}

static void syscall_exit_thread(void)
{
    thread_exit();
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
    case SYSCALL_KEYDOWN:
        regs->eax = syscall_keydown(regs->ebx);
        break;
    case SYSCALL_EXIT:
        syscall_exit(regs->ebx & 0xff);
        panic("Unreachable code");
    case SYSCALL_EXIT_THREAD:
        syscall_exit_thread();
        panic("Unreachable code");
    default:
        regs->eax = (uint32_t)-1;
        break;
    }

    if (thread_current()->process->exit_requested)
        thread_exit();
}

static void syscall_kbd_observer(void*)
{
    struct KeyEvent evt;
    if (!ps2kbd_get_event(&evt))
        return;

    if (evt.type == KEY_PRESSED) {
        bitset_set(&kbd_bitset, evt.scancode);
        // trace("KEY_PRESSED 0x%x", evt.scancode);
    } else {
        bitset_unset(&kbd_bitset, evt.scancode);
        // trace("KEY_RELEASED 0x%x", evt.scancode);
    }
}

void syscall_init(void)
{
    idt_set_handler(0x80, syscall_handler, 3);
    ps2kbd_add_observer(syscall_kbd_observer, NULL);

    size_t bitset_count = 65536;
    size_t bitset_len = bitset_storage_size(bitset_count);
    uint32_t* bitset_storage = kmalloc(bitset_len);
    if (!bitset_storage)
        panic("Out of memory");
    bitset_init(&kbd_bitset, bitset_storage, bitset_count);
}

