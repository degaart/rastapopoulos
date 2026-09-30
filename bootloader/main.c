#include <rastaldr.h>
#include <fat12.h>
#include <allocator.h>
#include <multiboot.h>
#include <elf.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


void _panic(const char* file, int line, const char* fmt, ...)
{
    printf("RASTALDR panic at %s:%d:\n", file, line);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    halt();
}

struct Regs
{
    uint16_t ax, bx, cx, dx;
    uint16_t si, di, bp, es;
    uint16_t cf;
};

void int13(struct Regs* regs);

unsigned int12()
{
    /* gcc-ia16 specifies bx, bp and flags should be preserved inside asm
     * blocks */
    unsigned ax;
    asm volatile("int 0x12\n\t" : "=a"(ax) : : "bx", "bp", "cc", "memory");
    return ax;
}

bool read_sector(const struct BPB* bpb, void* buffer, unsigned lba)
{
    /*
     * C = LBA / (HeadCount * Spt)
     * H = (LBA / Spt) % HeadCount
     * S = (LBA % Spt) + 1
     * CX = ((C & 0xff) << 8)|((C & 0x300) >> 2)|S
     */
    unsigned cyl = lba / (bpb->head_count * bpb->sectors_per_track);
    unsigned head = (lba / bpb->sectors_per_track) % bpb->head_count;
    unsigned sect = (lba % bpb->sectors_per_track) + 1;

    struct Regs regs = {0};
    regs.ax = 0x201;
    regs.bx = (uint16_t)buffer;
    regs.cx = ((cyl & 0xff) << 8) | ((cyl & 0x300) >> 2) | sect;
    regs.dx = bpb->boot_drive | (head << 8);
    regs.es = 0x0;

    int13(&regs);
    return regs.cf == 0;
}

bool read_sectors(const struct BPB* bpb, void* buffer, uint16_t lba,
                  uint16_t count)
{
    uint8_t* ptr = buffer;
    while (count--) {
        if (!read_sector(bpb, ptr, lba)) {
            return false;
        }
        lba++;
        ptr += bpb->bytes_per_sector;
    }

    return true;
}

void read_fully(struct File* file, size_t offset, void* buffer, size_t size)
{
    if (fat12_seek(file, offset) != offset) {
        panic("Seek failed");
    }

    void* ptr = buffer;
    while (size) {
        int nread = fat12_read(file, ptr, size);
        if (nread == -1) {
            panic("I/O error");
        } else if(nread == 0) {
            panic("Unexpected EOF");
        }

        size -= nread;
        ptr += nread;
    }
}

void main()
{
    /* Get conventional memory size */
    unsigned long mem_size = int12() * 1024UL;
    printf("Conventional memory: %lu bytes\n", mem_size);

    /*
     * Setup heap, notice our total address space is just 64Kb,
     * notice we don't care about the stack at all, so this will behave
     * strangely when allocating too much memory :)
     */
    extern void* __bss_end;
    size_t heap_size =
        (mem_size > 65535 ? 65535 : mem_size) - (uintptr_t)&__bss_end;
    printf("Heap: %p %u bytes\n", &__bss_end, heap_size);
    heap_init(&__bss_end, heap_size);

    /* Get boot drive number, stored in the BPB at 0x7c00 */
    const struct BPB* bpb = (const struct BPB*)0x7c00;
    uint8_t boot_drive = bpb->boot_drive;
    printf("Boot drive: %p %d\n", &boot_drive, (int)boot_drive);

    /* Validate sector size (powers of two from 512 to 4096) */
    if (bpb->bytes_per_sector < 512 || bpb->bytes_per_sector > 4096 ||
        (bpb->bytes_per_sector & (bpb->bytes_per_sector - 1)) != 0) {
        panic("Invalid sector size: %u", bpb->bytes_per_sector);
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

    /*
     * Multiboot1 header: fits within the first 8kb of the image,
     * and must be dword-aligned
     * So, read the first 8kb of the image
     */
    size_t remaining = 8192;
    void* read_buffer = malloc(remaining);
    void* read_ptr = read_buffer;
    while (remaining) {
        int nread = fat12_read(&file, read_ptr, remaining);
        if (nread == -1) {
            panic("I/O error");
        } else if (nread == 0) {
            break;
        }
        remaining -= nread;
        read_ptr += nread;
    }

    /* Sliding-window search of the multiboot signature */
    const struct multiboot_header* hdr = NULL;
    for (read_ptr = read_buffer; read_ptr < read_buffer + 8192 - sizeof(struct multiboot_header); read_ptr += 4) {
        const struct multiboot_header* ptr = read_ptr;
        if (ptr->magic == MULTIBOOT_HEADER_MAGIC) {
            if (ptr->flags + ptr->magic + ptr->checksum != 0) {
                panic("Invalid multiboot checksum");
            }
            hdr = ptr;
            break;
        }
    }

    if (!hdr) {
        panic("Multiboot header not found");
    }
    if (hdr->flags != (MULTIBOOT_PAGE_ALIGN|MULTIBOOT_MEMORY_INFO)) {
        panic("Unsupported multiboot flags: 0x%lx", hdr->flags);
    }

    /* Now, we need to parse the elf file, while not reading all of it into memory */
    const Elf32_Ehdr* elf_hdr = read_buffer;
    if (elf_hdr->e_ident[0] != 0x7f ||
            elf_hdr->e_ident[1] != 'E' ||
            elf_hdr->e_ident[2] != 'L' ||
            elf_hdr->e_ident[3] != 'F') {
        panic("Invalid ELF magic");
    } else if (elf_hdr->e_ident[EI_CLASS] != ELFCLASS32 ||
            elf_hdr->e_ident[EI_DATA] != ELFDATA2LSB ||
            elf_hdr->e_ident[EI_VERSION] != EV_CURRENT ||
            elf_hdr->e_type != ET_EXEC ||
            elf_hdr->e_machine != EM_386 ||
            elf_hdr->e_version != EV_CURRENT ||
            elf_hdr->e_phoff == 0) {
        panic("Unsupported ELF file");
    }

    size_t phdrs_size = elf_hdr->e_phentsize * elf_hdr->e_phnum;
    Elf32_Phdr* elf_phdrs = malloc(phdrs_size);
    read_fully(&file, elf_hdr->e_phoff, elf_phdrs, phdrs_size);
    for (int i = 0; i < elf_hdr->e_phnum; i++) {
        if (elf_phdrs[i].p_type == PT_LOAD) {
            printf("%d off=0x%lx vaddr=0x%lx fsize=0x%lx msize=0x%lx align=0x%lx\n",
                   i,
                   elf_phdrs[i].p_offset,
                   elf_phdrs[i].p_vaddr,
                   elf_phdrs[i].p_filesz,
                   elf_phdrs[i].p_memsz,
                   elf_phdrs[i].p_align);
        }
    }

    printf("OK\n");
}

