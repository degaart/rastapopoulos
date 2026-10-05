#include "a20.h"
#include "gdt.h"
#include "hmemcpy.h"
#include <allocator.h>
#include <elf.h>
#include <fat12.h>
#include <multiboot.h>
#include <rastaldr.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

extern void exec_kernel(uint32_t entry) __attribute__((noreturn));

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
        } else if (nread == 0) {
            panic("Unexpected EOF");
        }

        size -= nread;
        ptr += nread;
    }
}

static void load_program(struct File* file, const Elf32_Phdr* phdr)
{
    if (fat12_seek(file, phdr->p_offset) != phdr->p_offset) {
        panic("Failed to seek to %lu", phdr->p_offset);
    }

    static const int buffer_size = 4096;
    uint8_t* buffer = malloc(buffer_size);
    uint32_t off = phdr->p_offset;
    for (uint32_t dst = phdr->p_vaddr; dst < phdr->p_vaddr + phdr->p_filesz;) {
        int nread = fat12_read(file, buffer, buffer_size);
        if (nread == 0) {
            panic("Unexpected EOF at offset 0x%lx", off);
        }
        hmemcpy(dst, buffer, nread);

        off += nread;
        dst += buffer_size;
    }
    free(buffer);
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

    /* Enable A20 */
    printf("A20 enabled: %s\n", a20_enabled() ? "yes" : "no");
    if (!a20_enabled()) {
        printf("Enabling A20 by calling BIOS\n");
        if (!a20_enable_bios() || !a20_enabled()) {
            printf("Enabling A20 using the fast method\n");
            a20_enable_fast();
            if (!a20_enabled()) {
                printf("Enabling A20 using the keyboard controller\n");
                a20_enable_8042();
            }
        }
    }

    if (!a20_enabled()) {
        panic("Failed to enable A20");
    }

    debugbreak();
    uint32_t canary = 0xDEADBEEF;
    hmemcpy(0x100000, &canary, sizeof(canary));

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
    uint8_t* read_buffer = malloc(remaining);
    uint8_t* read_ptr = read_buffer;
    while (remaining) {
        int nread = fat12_read(&file, read_ptr, remaining);
        if (nread == -1) {
            panic("I/O error");
        } else if (nread == 0) {
            break;
        }

        printf("nread: %d\n", nread);

        remaining -= nread;
        read_ptr += nread;
    }

    for (int i = 0x1000; i < 0x1000 + 16; i++) {
        printf("%02x ", read_buffer[i]);
    }
    printf("\n");

    /* Sliding-window search of the multiboot signature */
    const struct multiboot_header* hdr = NULL;
    for (read_ptr = read_buffer;
         read_ptr < read_buffer + 8192 - sizeof(struct multiboot_header);
         read_ptr += 4) {
        const struct multiboot_header* ptr =
            (const struct multiboot_header*)read_ptr;
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
    if (hdr->flags != (MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO)) {
        panic("Unsupported multiboot flags: 0x%lx", hdr->flags);
    }

    /* Now, we need to parse the elf file, while not reading all of it into
     * memory */
    const Elf32_Ehdr* elf_hdr = (const Elf32_Ehdr*)read_buffer;
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
        if (elf_phdrs[i].p_type == PT_LOAD) {
            load_program(&file, &elf_phdrs[i]);
        }
    }

    /* setup GDT */
    struct Gdt gdt;
    gdt_set_entry(&gdt.null, 0, 0, 0, 0);
    gdt_set_entry(&gdt.code, 0, 0xfffff, GDT_ACCESS_CODE, GDT_FLAGS_32BIT_4K);
    gdt_set_entry(&gdt.data, 0, 0xfffff, GDT_ACCESS_DATA, GDT_FLAGS_32BIT_4K);

    struct Gdtr gdtr = {.limit = sizeof(gdt) - 1,
                        .base = (uint32_t)(uintptr_t)&gdt};
    gdt_load(&gdtr);

    /* Jump into kernel */
    exec_kernel(elf_hdr->e_entry);
    printf("OK\n");
}

