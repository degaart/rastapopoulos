#pragma once

#include <stdint.h>

#define EI_MAG0         0       /* File identification */
#define EI_MAG1         1       /* File identification */
#define EI_MAG2         2       /* File identification */
#define EI_MAG3         3       /* File identification */
#define EI_CLASS        4       /* File class */
#define EI_DATA         5       /* Data encoding */
#define EI_VERSION      6       /* File version */
#define EI_OSABI        7       /* Operating system/ABI identification */
#define EI_ABIVERSION   8       /* ABI version */
#define EI_PAD          9       /* Start of padding bytes */
#define EI_NIDENT       16      /* Size of e_ident[] */

#define ET_NONE     0       /* No file type */
#define ET_REL      1       /* Relocatable file */
#define ET_EXEC     2       /* Executable file */
#define ET_DYN      3       /* Shared object file */
#define ET_CORE     4       /* Core file */
#define ET_LOPROC   0xff00  /* Processor-specific */
#define ET_HIPROC   0xffff  /* Processor-specific */

#define EM_NONE         0   /* No machine */
#define EM_SPARC        2   /* SPARC */
#define EM_386          3   /* Intel 80386 */
#define EM_SPARC32PLUS  18  /* Sun SPARC 32+ */
#define EM_SPARCV9      43  /* SPARC V9 */
#define EM_AMD64        62  /* AMD 64 */

#define EV_NONE 0

#define ELFMAG0         0x7f
#define ELFMAG1         'E'
#define ELFMAG2         'L'
#define ELFMAG3         'F'


#define ELFCLASSNONE    0       /* Invalid class */
#define ELFCLASS32      1       /* 32–bit objects */
#define ELFCLASS64      2       /* 64–bit objects */

#define ELFDATANONE     0
#define ELFDATA2LSB     1
#define ELFDATA2MSB     2

#define SHN_UNDEF       0
#define SHN_LORESERVE   0xff00
#define SHN_LOPROC      0xff00
#define SHN_BEFORE      0xff00
#define SHN_AFTER       0xff01
#define SHN_AMD64_LCOMMON 0xff02
#define SHN_HIPROC      0xff1f
#define SHN_LOOS        0xff20
#define SHN_LOSUNW      0xff3f
#define SHN_SUNW_IGNORE 0xff3f
#define SHN_HISUNW      0xff3f
#define SHN_HIOS        0xff3f
#define SHN_ABS         0xfff1
#define SHN_COMMON      0xfff2
#define SHN_XINDEX      0xffff
#define SHN_HIRESERVE   0xffff

#define SHT_NULL            0
#define SHT_PROGBITS        1
#define SHT_SYMTAB          2
#define SHT_STRTAB          3
#define SHT_RELA            4
#define SHT_HASH            5
#define SHT_DYNAMIC         6
#define SHT_NOTE            7
#define SHT_NOBITS          8
#define SHT_REL             9
#define SHT_SHLIB           10
#define SHT_DYNSYM          11
#define SHT_INIT_ARRAY      14
#define SHT_FINI_ARRAY      15
#define SHT_PREINIT_ARRAY   16
#define SHT_GROUP           17
#define SHT_SYMTAB_SHNDX    18
#define SHT_LOOS            0x60000000

#define SHF_WRITE               0x1
#define SHF_ALLOC               0x2
#define SHF_EXECINSTR           0x4
#define SHF_MERGE               0x10
#define SHF_STRINGS             0x20
#define SHF_INFO_LINK           0x40
#define SHF_LINK_ORDER          0x80
#define SHF_OS_NONCONFORMING    0x100
#define SHF_GROUP               0x200
#define SHF_TLS                 0x400
#define SHF_MASKOS              0x0ff00000
#define SHF_AMD64_LARGE         0x10000000
#define SHF_ORDERED             0x40000000
#define SHF_EXCLUDE             0x80000000
#define SHF_MASKPROC            0xf0000000

#define PT_NULL                 0
#define PT_LOAD                 1
#define PT_DYNAMIC              2
#define PT_INTERP               3
#define PT_NOTE                 4
#define PT_SHLIB                5
#define PT_PHDR                 6
#define PT_TLS                  7

struct Elf32_Ehdr {
    unsigned char e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf32_Shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
};

struct Elf32_Sym {
    uint32_t st_name;
    uint32_t st_value;
    uint32_t st_size;
    unsigned char st_info;
    unsigned char st_other;
    uint16_t st_shndx;
};

struct Elf32_Phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};

