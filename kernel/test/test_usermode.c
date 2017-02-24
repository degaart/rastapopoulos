#include "../debug.h"
#include "../util.h"
#include "../vmm.h"
#include "../pmm.h"
#include "../registers.h"

void usermode_program();
void ring3jmp(uint32_t esp,
              uint32_t eflags,
              uint32_t eip,
              uint32_t edi,
              uint32_t esi,
              uint32_t edx,
              uint32_t ecx,
              uint32_t ebx,
              uint32_t eax,
              uint32_t ebp);

void test_usermode()
{
    trace("Testing usermode");

    /* Map usermode_program into ring3 page */
    assert(IS_ALIGNED((uint32_t)usermode_program, PAGE_SIZE));
    vmm_remap((uint32_t)usermode_program, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE | VMM_PAGE_USER);

    /* Map a stack for our program at 32Mb */
    uint32_t stack_page = pmm_alloc();
    unsigned char* stack = (unsigned char*) 0x2000000;
    vmm_map((uint32_t)stack, stack_page, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE | VMM_PAGE_USER);

    stack += PAGE_SIZE - 1;

    /* Jump into our program */
    uint32_t eflags = read_eflags();
    ring3jmp((uint32_t)stack,
             eflags,
             (uint32_t)usermode_program,
             0,
             0,
             0,
             0,
             0,
             0,
             0);
    assert(!"Usermode jump failed!");
}


