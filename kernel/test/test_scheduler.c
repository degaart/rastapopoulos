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

typedef void (*elf_entry_t)(void);

static elf_entry_t load_elf(const void* data)
{
    const unsigned char* file_data = data;

    /*
     * Check ELF file headers
     */
    Elf32_Ehdr* ehdr = (Elf32_Ehdr*)file_data;
    assert(ehdr->e_ident[EI_MAG0] == 0x7F);
    assert(ehdr->e_ident[EI_MAG1] == 'E');
    assert(ehdr->e_ident[EI_MAG2] == 'L');
    assert(ehdr->e_ident[EI_MAG3] == 'F');
    assert(ehdr->e_ident[EI_CLASS] == ELFCLASS32);
    assert(ehdr->e_ident[EI_DATA] == ELFDATA2LSB);
    assert(ehdr->e_type == ET_EXEC);
    assert(ehdr->e_machine == EM_386);
    assert(ehdr->e_version == EV_CURRENT);
    assert(ehdr->e_entry != NULL);

    /*
     * Load program segments
     */
    Elf32_Phdr* phdrs = (Elf32_Phdr*)(file_data + ehdr->e_phoff);
    assert(ehdr->e_phentsize == sizeof(Elf32_Phdr));
    for(int i = 0; i < ehdr->e_phnum; i++) {
        trace("Loading segment %d", i);

        Elf32_Phdr* phdr = phdrs + i;
        trace("\ttype: %p, offset: %p, vaddr: %p, filesz: %p, memsz: %p, flags: %p, align: %p",
              phdr->p_type,
              phdr->p_offset,
              phdr->p_vaddr,
              phdr->p_filesz,
              phdr->p_memsz,
              phdr->p_flags,
              phdr->p_align);

        if(phdr->p_type == PT_LOAD) {
            assert(phdr->p_memsz != 0);

            unsigned page_flags = VMM_PAGE_PRESENT | VMM_PAGE_USER;
            if(phdr->p_flags & PF_W)
                page_flags |= VMM_PAGE_WRITABLE;

            unsigned char* segment_start = phdr->p_vaddr;
            unsigned char* segment_end = segment_start + phdr->p_memsz;
            assert(segment_start >= (unsigned char*)USER_START);
            assert(segment_end <= (unsigned char*)USER_END);

            for(unsigned char* page = segment_start; page < segment_end; page += PAGE_SIZE) {
                vmm_map(page,
                        pmm_alloc(),
                        VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
            }

            bzero(segment_start, phdr->p_memsz);
            if(phdr->p_filesz) {
                memcpy(segment_start,
                       file_data + phdr->p_offset,
                       phdr->p_filesz);
            }

            for(unsigned char* page = segment_start; page < segment_end; page += PAGE_SIZE) {
                vmm_remap(page, page_flags);
            }
        }
    }
    return ehdr->e_entry;
}

static uint32_t syscall_exec_handler(struct isr_regs* regs)
{
    const char* filename = (const char*)regs->ebx;
    assert(filename);
    assert(*filename);

    trace("(%d %s) exec: %s", 
          current_task_pid(), 
          current_task_name(), 
          filename);

    /* Load file from initrd */
    const struct initrd_file* file = initrd_get_file(filename);
    if(!file)
        return 0;

    /* Unmap process memory */
    for(unsigned char* va = (unsigned char*)USER_START;
        va < USER_STACK; 
        va += PAGE_SIZE) {

        assert(va != USER_STACK);
        uint32_t pa = vmm_get_physical(va);
        if(pa) {
            vmm_unmap(va);
            pmm_free(pa);
        }
    }

    /* Load elf file */
    elf_entry_t entry = load_elf(file->data);

    /* Reset process state */
    current_task_set_name(filename);
    regs->esp = (uint32_t)(USER_STACK + PAGE_SIZE);
    regs->useresp = (uint32_t)(USER_STACK + PAGE_SIZE);
    regs->eflags = read_eflags() | EFLAGS_IF;
    regs->eip = (uint32_t)entry;

    return 0;
}

static void test_load_initrd()
{
    syscall_register(SYSCALL_EXEC, syscall_exec_handler);

    /*
     * Load ELF file
     */
    const struct initrd_file* init_file = initrd_get_file("init.elf");
    assert(init_file != NULL);

    elf_entry_t entry = load_elf(init_file->data);

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



