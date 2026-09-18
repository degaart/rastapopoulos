#include "allocator.h"
#include "format.h"
#include "multiboot.h"
#include "string.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern void halt(void);

#define debugbreak() asm volatile("xchg bx, bx" ::: "memory")

#define assert(cond)                                                          \
    if (!(cond)) {                                                            \
        printf("Assert failed at %s:%u\n%s\n", __FILE__, __LINE__, #cond);    \
        halt();                                                               \
    }

struct Regs
{
    uint16_t ax, bx, cx, dx;
    uint16_t si, di, bp, es;
    uint16_t cf;
};

void int13(struct Regs* regs);

struct BPB
{
    uint8_t jump[3];
    uint8_t oem_name[8];
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

#define FAT12_FREE     0xe5
#define FAT12_FIRST    2
#define FAT12_RSVD     0xff0
#define FAT12_RSVD_END 0xff6
#define FAT12_BAD      0xff7
#define FAT12_END      0xff8
struct Dirent
{
    char filename[8];         /* Filename (padded with spaces) */
    char ext[3];              /* Extension (padded with spaces) */
    uint8_t attributes;       /* File attributes (hidden, system, dir, etc.) */
    uint8_t reserved_nt;      /* Reserved for NT (lowercase flags) */
    uint8_t creation_time_ms; /* Creation time tenths of a second */
    uint16_t creation_time;   /* Creation time (Hour:5, Min:6, Sec:5/2) */
    uint16_t creation_date;   /* Creation date (Year:7, Month:4, Day:5) */
    uint16_t last_access_date;  /* Last access date (Year:7, Month:4, Day:5) */
    uint16_t first_cluster_hi;  /* High 16-bits of cluster (0 in FAT12) */
    uint16_t write_time;        /* Last modification time */
    uint16_t write_date;        /* Last modification date */
    uint16_t first_cluster_low; /* Low 16-bits of cluster (Starting cluster) */
    uint32_t file_size;         /* File size in bytes */
} __attribute__((packed));

struct File
{
    uint32_t size;
    uint32_t offset;
    uint16_t current_cluster;
};

void putc(int ch)
{
    asm volatile("cmp al, 10\n\t"
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
                 : "bx", "bp", "cc", "memory");
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

bool read_sectors(const struct BPB* bpb, void* buffer, unsigned lba,
                  unsigned count)
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

bool read_cluster(const struct BPB* bpb, void* buffer, unsigned cluster)
{
    uint16_t first_data_lba =
        bpb->reserved_sectors + (bpb->fat_count * bpb->sectors_per_fat) +
        ((bpb->root_entries * 32 + bpb->bytes_per_sector - 1) /
         bpb->bytes_per_sector);
    uint16_t lba = first_data_lba + (cluster - 2) * bpb->sectors_per_cluster;

    return read_sectors(bpb, buffer, lba, bpb->sectors_per_cluster);
}

void main()
{
    /* Get conventional memory size */
    unsigned long mem_size = int12() * 1024UL;
    printf("Conventional memory: %lu bytes\n", mem_size);

    /*
     * Setup heap, notice our total address space is just 64Kb,
     * notice we don't care about the stack at all, so this will
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
        printf("Invalid sector size: %u\n", bpb->bytes_per_sector);
    }

    /* Read FAT */
    size_t fat_size = bpb->sectors_per_fat * bpb->bytes_per_sector;
    uint8_t* fat_buffer = malloc(fat_size);
    bool ret = read_sectors(bpb, fat_buffer, 0x1, bpb->sectors_per_fat);
    if (!ret) {
        printf("Failed to read FAT\n");
        halt();
    }

    /* read root dir */
    unsigned root_dir_lba =
        bpb->reserved_sectors + (bpb->fat_count * bpb->sectors_per_fat);
    unsigned root_dir_sectors =
        (bpb->root_entries * 32) / bpb->bytes_per_sector;
    struct Dirent* root_dir_buffer =
        malloc(root_dir_sectors * bpb->bytes_per_sector);
    ret = read_sectors(bpb, root_dir_buffer, root_dir_lba, root_dir_sectors);
    if (!ret) {
        printf("Failed to read root dir\n");
        halt();
    }

    /* Find KERNEL.ELF in root dir */
    const struct Dirent* entry = root_dir_buffer;
    const struct Dirent* kernel_entry = NULL;
    for (; entry->filename[0]; entry++) {
        if (entry->filename[0] == FAT12_FREE) {
            continue;
        } else if (!memcmp(entry->filename, "KERNEL  ELF", 8 + 3)) {
            kernel_entry = entry;
            break;
        }
    }

    if (!kernel_entry) {
        printf("KERNEL.ELF not found\n");
        halt();
    } else if (kernel_entry->file_size < 512) {
        printf("Invalid kernel, size=%lu bytes\n", kernel_entry->file_size);
        halt();
    }

    void* kernel_buffer = malloc((size_t)kernel_entry->file_size);
    uint8_t* kernel_ptr = kernel_buffer;
    printf("kernel_ptr: %p\n", kernel_ptr);
    printf("Free mem: %u bytes\n", heap_info());

    /* Walk cluster chain */
    uint16_t cluster = kernel_entry->first_cluster_low;
    while (true) {
        if (cluster < FAT12_FIRST || cluster == FAT12_BAD ||
            (cluster >= FAT12_RSVD && cluster <= FAT12_RSVD_END)) {
            printf("Invalid cluster detected\n");
            halt();
        } else if (cluster >= FAT12_END) {
            break;
        }

        if (!read_cluster(bpb, kernel_ptr, cluster)) {
            printf("Failed to read cluster %u\n", cluster);
            halt();
        }
        kernel_ptr += bpb->sectors_per_cluster * bpb->bytes_per_sector;

        /* Next cluster */
        size_t offset = cluster + (cluster / 2);
        uint16_t value =
            fat_buffer[offset] | ((uint16_t)fat_buffer[offset + 1] << 8);
        if (cluster & 1) { /* odd */
            cluster = value >> 4;
        } else { /* even */
            cluster = value & 0xfff;
        }
    }

    /*
     * Multiboot1 header: fits within the first 8kb of the image,
     * and must be dword-aligned
     * So, read the first 8kb of the image
     */
    struct File file;
    file.size = entry->file_size;
    file.offset = 0;
    file.current_cluster = entry->first_cluster_low;

    printf("OK\n");
}

