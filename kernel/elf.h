#ifndef _ELF_H_
#define _ELF_H_

#include <stdint.h>

typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef int32_t Elf32_Sword;
typedef uint32_t Elf32_Word;

#define EI_NIDENT 16

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    Elf32_Half e_type;
    Elf32_Half e_machine;
    Elf32_Word e_version;
    Elf32_Addr e_entry;
    Elf32_Off e_phoff;
    Elf32_Off e_shoff;
    Elf32_Word e_flags;
    Elf32_Half e_ehsize;
    Elf32_Half e_phentsize;
    Elf32_Half e_phnum;
    Elf32_Half e_shentsize;
    Elf32_Half e_shnum;
    Elf32_Half e_shstrndx;
} __attribute__((packed)) Elf32_Ehdr;

// e_type
#define ET_NONE         0
#define ET_REL          1
#define ET_EXEC         2
#define ET_DYN          3
#define ET_CORE         4
#define ET_LOPROC       0xFF00
#define ET_HIPROC       0xFFFF

// e_machine
#define EM_NONE         0
#define EM_386          3

// e_version
#define EV_NONE         0
#define EV_CURRENT      1

// e_ident
#define EI_MAG0         0
#define EI_MAG1         1
#define EI_MAG2         2
#define EI_MAG3         3
#define EI_CLASS        4
#define EI_DATA         5
#define EI_VERSION      6
#define EI_PAD          7

// ei_magx
#define ELFMAG0         0x7F
#define ELFMAG1         'E'
#define ELFMAG2         'L'
#define ELFMAG3         'F'

// ei_class
#define ELFCLASSNONE    0
#define ELFCLASS32      1
#define ELFCLASS64      2

// ei_data
#define ELFDATANONE     0
#define ELFDATA2LSB     1
#define ELFDATA2MSB     2

typedef struct {
    Elf32_Word  sh_name;
    Elf32_Word  sh_type;
    Elf32_Word  sh_flags;
    Elf32_Addr  sh_addr;
    Elf32_Off   sh_offset;
    Elf32_Word  sh_size;
    Elf32_Word  sh_link;
    Elf32_Word  sh_info;
    Elf32_Word  sh_addralign;
    Elf32_Word  sh_entsize;
} __attribute__((packed)) Elf32_Shdr;

// special section indices
#define SHN_UNDEF       0

// sh_type
#define SHT_NULL        0
#define SHT_SYMTAB      2
#define SHT_STRTAB      3
#define SHT_RELA        4
#define SHT_DYNAMIC     5
#define SHT_REL         9
#define SHT_DYNSYM      11

// sh_flags
#define SHF_WRITE       0x1
#define SHF_ALLOC       0x2
#define SHF_EXECINSTR   0x4

typedef struct {
    Elf32_Word p_type;
    Elf32_Off p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
} __attribute__((packed)) Elf32_Phdr;

// p_type
#define PT_NULL         0
#define PT_LOAD         1
#define PT_DYNAMIC      2
#define PT_INTERP       3
#define PT_NOTE         4
#define PT_SHLIB        5
#define PT_PHDR         6
#define PT_GNUSTACK     0x6474E551
#define PT_LOPROC       0x70000000
#define PT_HIPROC       0x7fffffff

// p_flags
#define PF_EXECUTABLE   1
#define PF_WRITABLE     2
#define PF_READABLE     4


#ifdef __cplusplus
extern "C" {
#endif
    
const Elf32_Phdr* elf_segment_header(const Elf32_Ehdr* hdr, unsigned index);
const Elf32_Shdr* elf_section_header(const Elf32_Ehdr* hdr, unsigned index);
const char* elf_section_name(const Elf32_Ehdr* hdr, unsigned index);
const Elf32_Ehdr* elf_validate(const void* buffer, uint32_t size);

#ifdef __cplusplus
}
#endif



#endif //_ELF_H_

