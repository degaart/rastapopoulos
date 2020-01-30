#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "kernel.h"
#include "io.h"
#include "halt.h"
#include "debug.h"
#include "reboot.h"
#include "string.h"
#include "registers.h"
#include "gdt.h"
#include "idt.h"
#include "kmalloc.h"

static void int80_handler(const struct isr_regs* regs)
{
    trace("Hello from int80");
}

void kmain()
{
    trace("");
    trace("*** Started ***");

    gdt_init();
    idt_init();

    trace("KERNEL_START: 0x%X", KERNEL_START);
    trace("KERNEL_END: 0x%X", KERNEL_END);

    kmalloc_init(KERNEL_END);

    trace("Allocating 3 bytes");
    unsigned char* p0 = kmalloc(3);
    trace("p0: 0x%X", p0);
    for(size_t i = 0; i < 9; i++)
        p0[i] = '-';


    trace("Now, allocating 4 bytes");
    unsigned char* p1 = kmalloc(4);
    trace("p1: 0x%X", p1);

    trace("Another 4 bytes");
    unsigned char* p2 = kmalloc(4);
    trace("p2: 0x%X", p2);

    trace("What about 16 bytes");
    unsigned char* p3 = kmalloc(4);
    trace("p3: 0x%X", p3);

    trace("Freeing...");
    kfree(p3);
    kfree(p2);
    kfree(p1);
    kfree(p0);

    trace("*** Stopped ***");
    reboot();
}


