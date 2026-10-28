#include "kernel.h"
#include "kthread.h"
#include <limits.h>

static struct Thread bootstrap_state;

void enter_critical_section(void)
{
    struct Thread* thread = thread_current();
    if (!thread) {
        thread = &bootstrap_state;
    }

    bool iflag = !!(read_eflags() & EFLAGS_IF);
    disable_interrupts();
    if (thread->critical_section_nesting == 0)
        thread->critical_section_flag = iflag;
    if (thread->critical_section_nesting == INT_MAX)
        panic("Critical section nesting depth exceeded");
    thread->critical_section_nesting++;
}

void leave_critical_section(void)
{
    if (read_eflags() & EFLAGS_IF)
        panic("leave_critical_section with interrupts enabled");

    struct Thread* thread = thread_current();
    if (!thread)
        thread = &bootstrap_state;
    if (thread->critical_section_nesting <= 0)
        panic("leave_critical_section with invalid depth");
    thread->critical_section_nesting--;
    if (thread->critical_section_nesting == 0 && thread->critical_section_flag)
        enable_interrupts();
}

