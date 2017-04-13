#include "util.h"
#include "string.h"
#include "registers.h"
#include "locks.h"
#include "list.h"
#include "syscalls.h"

#include "../process.h"
#include "../kdebug.h"
#include "../pmm.h"
#include "../vmm.h"
#include "../kernel.h"
#include "../gdt.h"
#include "../context.h"
#include "../kmalloc.h"
#include "../timer.h"
#include "../common/io.h"
#include "../syscall.h"
#include "../ipc.h"
#include "../scheduler.h"
#include "../initrd.h"
#include "../elf.h"


static void test_load_initrd()
{
    /*
     * Load ELF file
     */
    const struct initrd_file* init_file = initrd_get_file("init.elf");
    assert(init_file != NULL);

    elf_entry_t entry = load_elf(init_file->data, init_file->size);

    trace("Starting execution at %p", entry);
    scheduler_start(entry);

    reboot();
}

/****************************************************************************
 * Initialization
 ****************************************************************************/
void test_scheduler()
{
    trace("Testing scheduler");

#if 1
    test_load_initrd();
#else
    scheduler_start(init_entry);
#endif
}



