#include "elf.h"
#include "kdebug.h"
#include "string.h"
#include "vmm.h"
#include "pmm.h"

elf_entry_t load_elf(const void* data, unsigned size)
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


