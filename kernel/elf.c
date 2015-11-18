#include <stddef.h>
#include "elf.h"

const Elf32_Phdr* elf_segment_header(const Elf32_Ehdr* hdr, unsigned index) {
    if (index >= hdr->e_phnum) {
        return NULL;
    }
    const Elf32_Phdr* phdr = (const Elf32_Phdr*)((uint8_t*)hdr + hdr->e_phoff + (hdr->e_phentsize * index));
    return phdr;
}

const Elf32_Shdr* elf_section_header(const Elf32_Ehdr* hdr, unsigned index) {
    if(index >= hdr->e_shnum) {
        return NULL;
    }
    
    const Elf32_Shdr* shdr = (const Elf32_Shdr*)((uint8_t*)hdr + hdr->e_shoff + (hdr->e_shentsize * index));
    return shdr;
}

const char* elf_section_name(const Elf32_Ehdr* hdr, unsigned index) {
    const Elf32_Shdr* section = elf_section_header(hdr, index);
    const Elf32_Shdr* str_section = elf_section_header(hdr, hdr->e_shstrndx);
    return (const char*)(hdr) + str_section->sh_offset + section->sh_name;
}

const Elf32_Ehdr* elf_validate(const void* buffer, uint32_t size) {
    if(size < sizeof(Elf32_Ehdr)) {
        return NULL;
    }
    
    const Elf32_Ehdr* hdr = (const Elf32_Ehdr*)buffer;
    if(hdr->e_ident[EI_MAG0] != ELFMAG0 || hdr->e_ident[EI_MAG1] != ELFMAG1 || hdr->e_ident[EI_MAG2] != ELFMAG2 || hdr->e_ident[EI_MAG3] != ELFMAG3) {
        return NULL;
    } else if(hdr->e_ident[EI_CLASS] != ELFCLASS32 || hdr->e_ident[EI_DATA] != ELFDATA2LSB || hdr->e_ident[EI_VERSION] != EV_CURRENT) {
        return NULL;
    } else if(hdr->e_type != ET_EXEC || hdr->e_machine != EM_386 || hdr->e_version != EV_CURRENT) {
        return NULL;
    }
    return hdr;
}
