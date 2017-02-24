#include "../debug.h"
#include "../util.h"
#include "../vmm.h"
#include "../pmm.h"
#include "../registers.h"
#include "../idt.h"
#include "../gdt.h"
#include "../kernel.h"

#define TEST_EDI        0xABCD0001
#define TEST_ESI        0xABCD0002
#define TEST_EDX        0xABCD0003
#define TEST_ECX        0xABCD0004
#define TEST_EBX        0xABCD0005
#define TEST_EAX        0xABCD0006
#define TEST_EBP        0xABCD0007

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

static void int80_handler(struct isr_regs* regs)
{
    trace("Back into ring0, esp: %p", read_esp());

    /* Check register values */
    assert(regs->edi == (TEST_EDI ^ 7));
    assert(regs->esi == (TEST_ESI ^ 7));
    assert(regs->edx == (TEST_EDX ^ 7));
    assert(regs->ecx == (TEST_ECX ^ 7));
    assert(regs->ebx == (TEST_EBX ^ 7));
    assert(regs->eax == (TEST_EAX ^ 7));
    assert(regs->ebp == (TEST_EBP ^ 7));

    reboot();
}

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

    /* Set kernel stack to initial_kernel_stack */
    tss_set_kernel_stack(initial_kernel_stack + PAGE_SIZE);

    /* Install int80 handler so we can return into kernel from ring3 */
    idt_install(0x80, int80_handler, true);

    /* Jump into our program */
    uint32_t eflags = read_eflags();
    ring3jmp((uint32_t)stack,                   /* esp */
             eflags,                            /* eflags */
             (uint32_t)usermode_program,        /* eip */
             TEST_EDI,                          /* edi */
             TEST_ESI,                          /* esi */
             TEST_EDX,                          /* edx */ 
             TEST_ECX,                          /* ecx */
             TEST_EBX,                          /* ebx */
             TEST_EAX,                          /* eax */
             TEST_EBP);                         /* ebp */
    assert(!"Usermode jump failed!");
}


