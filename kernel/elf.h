#pragma once

typedef struct {
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
} elf32_shdr_t;

#define SHT_NULL                0   
#define SHT_SYMTAB              2
#define SHT_STRTAB              3

typedef struct {
    uint32_t st_name;
    uint32_t st_value;
    uint32_t st_size;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
} elf32_sym_t;

#define STN_UNDEF               0

#define STT_NOTYPE              0
#define STT_OBJECT              1
#define STT_FUNC                2
#define STT_SECTION             3
#define STT_FILE                4
#define STT_LOPROC              13
#define STT_HIPROC              15

#define ELF32_ST_BIND(i)        ((i) >> 4)
#define ELF32_ST_TYPE(i)        ((i) & 0xF)
#define ELF32_ST_INFO(b, t)     (((b) << 4) | ((t) & 0xF))

