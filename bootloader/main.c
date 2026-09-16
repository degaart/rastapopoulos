#include <stddef.h>
#include <stdint.h>
#include "format.h"

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
};

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

void main()
{
    /* Get conventional memory size */
    unsigned long mem_size = int12() * 1024UL;
    printf("Conventional memory: %lu bytes\n", mem_size);

    /* Get boot drive number, stored in the BPB at 0x7c00 */
    const struct BPB* bpb = (const struct BPB*)0x7c00;
    uint8_t boot_drive = bpb->boot_drive;
    printf("Boot drive: %d\n", (int)boot_drive);

    /* Validate sector size (powers of two from 512 to 4096) */
    if (bpb->bytes_per_sector < 512 || bpb->bytes_per_sector > 4096 ||
        (bpb->bytes_per_sector & (bpb->bytes_per_sector - 1)) != 0)
    {
        printf("Invalid sector size: %u\n", bpb->bytes_per_sector);
    }

    /* Read FAT */
}


