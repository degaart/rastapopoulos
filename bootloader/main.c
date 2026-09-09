#include "allocator.h"
#include "format.h"
#include "rastaldr.h"
#include <stddef.h>
#include <stdint.h>

#define MK_FP(seg, off)                                                       \
    ((void __far*)(((uint32_t)(uint16_t)(seg) << 16) | (uint16_t)(off)))

struct BPB
{
    char oemName[8];
    uint16_t bytesPerSector;
    uint8_t sectorsPerCluster;
    uint16_t reservedSectors;
    uint8_t fatCount;
    uint16_t rootEntries;
    uint16_t totalSectors16;
    uint8_t mediaDescriptor;
    uint16_t sectorsPerFat;
    uint16_t sectorsPerTrack;
    uint16_t headCount;
    uint32_t hiddenSectors;
    uint32_t totalSectors32;
    uint8_t bootDrive;
    uint8_t reserved;
    uint8_t extendedSignature;
    uint32_t volumeSerial;
    char volumeLabel[12];
    char filesystemType[8];
} __attribute__((packed));

struct FATDirEntry
{
    uint8_t  name[8];         /* 0x00: space-padded base name */
    uint8_t  extension[3];    /* 0x08: space-padded extension */
    uint8_t  attributes;      /* 0x0B */
    uint8_t  reserved;        /* 0x0C: NT case flags */
    uint8_t  creationMS;      /* 0x0D: creation time tenths */
    uint16_t creationTime;    /* 0x0E */
    uint16_t creationDate;    /* 0x10 */
    uint16_t accessDate;      /* 0x12 */
    uint16_t clusterHigh;     /* 0x14: always zero on FAT12/16 */
    uint16_t modifiedTime;    /* 0x16 */
    uint16_t modifiedDate;    /* 0x18 */
    uint16_t clusterLow;      /* 0x1A: first cluster */
    uint32_t fileSize;        /* 0x1C: bytes */
} __attribute__((packed));

extern void* __bss_end;

static void writeChar(void* data, char ch)
{
    putc(ch);
}

int printf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vformat(writeChar, NULL, fmt, args);
    va_end(args);
    return ret;
}

void printString(const char* str, size_t len)
{
    while(len-- && *str)
    {
        putc(*str);
        str++;
    }
}

#define panic(...) \
    do { \
        printf("%s:%d RASTALDR panic: ", __FILE__, __LINE__); \
        printf(__VA_ARGS__); \
        halt(); \
    } while(0)

extern uint16_t read_sectors_chs(void* buffer, uint16_t count, uint16_t cyl,
                                 uint16_t head, uint16_t sect, uint16_t drive);

uint16_t readSectors(void* buffer, unsigned count, unsigned lba)
{
    static const struct BPB __far* bpb = MK_FP(0x0, 0x7c03);
    uint16_t cyl = lba / (bpb->headCount * bpb->sectorsPerTrack);
    uint16_t head = (lba / bpb->sectorsPerTrack) % bpb->headCount;
    uint16_t sect = (lba % bpb->sectorsPerTrack) + 1;
    uint16_t ret =
        read_sectors_chs(buffer, count, cyl, head, sect, bpb->bootDrive);

    return ret >> 8;
}

void ldrmain(void)
{
    size_t heapSize = 0xffff - (size_t)&__bss_end;
    heapInit(&__bss_end, heapSize);
    printf("Heap: %p - %u bytes\n", &__bss_end, heapSize);

    /* Boot parameter block is at 0x7c03 */
    static const struct BPB __far* bpb = MK_FP(0x0, 0x7c03);
    printf("Boot drive: %u\n", bpb->bootDrive);
    printf("sizeof(int): %u\n", sizeof(int));
    printf("sizeof(void*): %u\n", sizeof(void*));

    uint16_t ds = __builtin_ia16_near_data_segment();
    printf("ds: 0x%x\n", ds);

    /* Read first FAT */
    uint8_t* fat = malloc(bpb->sectorsPerFat * bpb->bytesPerSector);
    if (!fat)
    {
        panic("Out of memory\n");
    }

    uint16_t ret = readSectors(fat, bpb->sectorsPerFat, 1);
    if (ret) {
        panic("Failed to read FAT: %u\n", ret);
    }

    /* Read root directory */
    size_t rootDirLen = bpb->rootEntries * 32;
    void* rootDir = malloc(rootDirLen);
    if (!rootDir)
    {
        panic("Out of memory\n");
    }
    ret = readSectors(rootDir,
                      rootDirLen / bpb->bytesPerSector,
                      bpb->reservedSectors + (bpb->fatCount * bpb->sectorsPerFat));
    if(ret) {
        panic("Failed to read root dir: %u\n", ret);
    }

    /* Iterate root entries */
    for (struct FATDirEntry* entry = rootDir; entry->name[0]; entry++)
    {
        printString(entry->name, sizeof(entry->name));
        printString(".", 1);
        printString(entry->extension, sizeof(entry->extension));
        printf(" %u\n", (unsigned)(entry->fileSize & 0xffff));
    }
}

