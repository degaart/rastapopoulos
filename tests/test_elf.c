#include "rastaldr_glue.h"

#include <allocator.h>
#include <elf.h>
#include <fat12.h>
#include <multiboot.h>
#include <rastaldr.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

void main()
{
    static const int HEAP_SIZE = 32768;
    void* heap = mmap(NULL, HEAP_SIZE, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (!heap) {
        panic("mmap failed");
    }

    heap_init(heap, HEAP_SIZE);

    const struct BPB* bpb = read_bpb();
    if (!bpb) {
        panic("Failed to read bpb");
    }

    /* Read FAT */
    size_t fat_size = bpb->sectors_per_fat * bpb->bytes_per_sector;
    uint8_t* fat_buffer = malloc(fat_size);
    bool ret = read_sectors(bpb, fat_buffer, 0x1, bpb->sectors_per_fat);
    if (!ret) {
        panic("Failed to read FAT");
    }

    /* Open kernel */
    struct File file;
    ret = fat12_open(bpb, fat_buffer, &file, "kernel.elf");
    if (!ret) {
        panic("Failed to open kernel.elf");
    }

    /* Now, we need to parse the elf file, while not reading all of it into
     * memory */
    Elf32_Ehdr* elf_hdr = malloc(sizeof(Elf32_Ehdr));
    read_fully(&file, 0, elf_hdr, sizeof(Elf32_Ehdr));
    if (elf_hdr->e_ident[0] != 0x7f || elf_hdr->e_ident[1] != 'E' ||
        elf_hdr->e_ident[2] != 'L' || elf_hdr->e_ident[3] != 'F') {
        panic("Invalid ELF magic");
    } else if (elf_hdr->e_ident[EI_CLASS] != ELFCLASS32 ||
               elf_hdr->e_ident[EI_DATA] != ELFDATA2LSB ||
               elf_hdr->e_ident[EI_VERSION] != EV_CURRENT ||
               elf_hdr->e_type != ET_EXEC || elf_hdr->e_machine != EM_386 ||
               elf_hdr->e_version != EV_CURRENT || elf_hdr->e_phoff == 0) {
        panic("Unsupported ELF file");
    }

    size_t phdrs_size = elf_hdr->e_phentsize * elf_hdr->e_phnum;
    Elf32_Phdr* elf_phdrs = malloc(phdrs_size);
    read_fully(&file, elf_hdr->e_phoff, elf_phdrs, phdrs_size);
    for (int i = 0; i < elf_hdr->e_phnum; i++) {
        printf("%u 0x%lx\n", i, elf_phdrs[i].p_type);
        if (elf_phdrs[i].p_type == PT_LOAD) {
            printf("%d off=0x%lx vaddr=0x%lx fsize=0x%lx msize=0x%lx "
                   "align=0x%lx\n",
                   i, elf_phdrs[i].p_offset, elf_phdrs[i].p_vaddr,
                   elf_phdrs[i].p_filesz, elf_phdrs[i].p_memsz,
                   elf_phdrs[i].p_align);
        }
    }

    printf("OK\n");
}

