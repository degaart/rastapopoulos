#include "allocator.h"
#include "format.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern void halt(void);

#define debugbreak() \
    asm volatile("xchg bx, bx":::"memory")

struct Regs
{
    uint16_t ax, bx, cx, dx;
    uint16_t si, di, bp, es;
    uint16_t cf;
};

void int13(struct Regs *regs);

struct BPB
{
    uint8_t jump[3];
    uint8_t  oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t media_descriptor;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint8_t boot_drive;
    uint8_t reserved;
    uint8_t extended_signature;
    uint32_t volume_serial;
    uint8_t volume_label[11];
    uint8_t filesystem_type[8];
} __attribute__((packed));

void putc(int ch)
{
    asm volatile (
        "cmp al, 10\n\t"
        "jne 1f\n\t"
        "push ax\n\t"
        "mov al, 13\n\t"
        "mov ah, 0xe\n\t"
        "mov bx, 0x7\n\t"
        "int 0x10\n\t"
        "pop ax\n\t"
        "1:\n\t"
        "mov ah, 0xe\n\t"
        "mov bx, 0x7\n\t"
        "int 0x10"
        : "+a"(ch)
        :
        : "bx", "bp", "cc", "memory"
    );
}

void format_out(void* unused, char ch)
{
    putc(ch);
}

int printf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vformat(format_out, NULL, fmt, args);
    va_end(args);
    return ret;
}

unsigned int12()
{
    /* gcc-ia16 specifies bx, bp and flags should be preserved inside asm blocks */
    unsigned ax;
    asm volatile(
        "int 0x12\n\t"
        : "=a"(ax)
        :
        : "bx", "bp", "cc", "memory"
    );
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
    regs.cx = ((cyl & 0xff) << 8)|((cyl & 0x300) >> 2)|sect;
    regs.dx = bpb->boot_drive|(head << 8);
    regs.es = 0x0;

    int13(&regs);
    return regs.cf == 0;
}

bool read_sectors(const struct BPB* bpb, void* buffer, unsigned lba, unsigned count)
{
    uint8_t* ptr = buffer;
    while(count--)
    {
        if (!read_sector(bpb, ptr, lba))
        {
            return false;
        }
        lba++;
        ptr+=bpb->bytes_per_sector;
    }

    return true;
}

void main()
{
    /* Get conventional memory size */
    unsigned long mem_size = int12() * 1024UL;
    printf("Conventional memory: %lu bytes\n", mem_size);

    /* Setup heap, notice out total address space is just 64Kb */
    extern void* __bss_end;
    size_t heap_size = (mem_size > 65535 ? 65535 : mem_size) - (uintptr_t)&__bss_end;
    printf("Heap: %p %u bytes\n", &__bss_end, heap_size);
    heap_init(&__bss_end, heap_size);

    /* Get boot drive number, stored in the BPB at 0x7c00 */
    const struct BPB* bpb = (const struct BPB*)0x7c00;
    uint8_t boot_drive = bpb->boot_drive;
    printf("Boot drive: %p %d\n", &boot_drive, (int)boot_drive);

    /* Validate sector size (powers of two from 512 to 4096) */
    if (bpb->bytes_per_sector < 512 || bpb->bytes_per_sector > 4096 ||
        (bpb->bytes_per_sector & (bpb->bytes_per_sector - 1)) != 0)
    {
        printf("Invalid sector size: %u\n", bpb->bytes_per_sector);
    }

    /* Read FAT */
    size_t fat_size = bpb->sectors_per_fat * bpb->bytes_per_sector;
    uint16_t* fat_buffer = malloc(fat_size);
    bool ret = read_sectors(bpb, fat_buffer, 0x1, bpb->sectors_per_fat);
    if (!ret)
    {
        printf("Failed to read FAT\n");
        halt();
    }

    /* read root dir */
    unsigned root_dir_lba = bpb->reserved_sectors + (bpb->fat_count * bpb->sectors_per_fat);
    unsigned root_dir_sectors = (bpb->root_entries * 32) / bpb->bytes_per_sector;
    uint16_t* root_dir_buffer = malloc(root_dir_sectors * bpb->bytes_per_sector);
    ret = read_sectors(bpb, root_dir_buffer, root_dir_lba, root_dir_sectors);
    if (!ret)
    {
        printf("Failed to read root dir\n");
        halt();
    }
    printf("OK\n");
}


