#include "syscall.h"
#include "idt.h"
#include "kthread.h"
#include "pit.h"

#define SYSCALL_GETTICKS 0
#define SYSCALL_SLEEP    1
#define SYSCALL_KEYDOWN  2

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
    return -1;
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
    default:
        regs->eax = (uint32_t)-1;
        break;
    }
}

void syscall_init(void)
{
    idt_set_handler(0x80, syscall_handler, 3);
}

