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

extern uint16_t readSectorsCHS(void* buffer, uint16_t count, uint16_t cyl,
                               uint16_t head, uint16_t sect, uint16_t drive);

void ldrmain(void)
{
    printf("ldrmain %d%d running\n", 420, 69);

    /* Boot parameter block is at 0x7c03 */
    static const struct BPB __far* bpb = MK_FP(0x0, 0x7c03);
    printf("Boot drive: %u\n", bpb->bootDrive);
    printf("sizeof(int): %u\n", sizeof(int));
    printf("sizeof(void*): %u\n", sizeof(void*));

    unsigned char readBuffer[512];
    uint16_t lba = 33;
    uint16_t cyl = lba / (bpb->headCount * bpb->sectorsPerTrack);
    uint16_t head = (lba / bpb->sectorsPerTrack) % bpb->headCount;
    uint16_t sect = (lba % bpb->sectorsPerTrack) + 1;
    uint16_t ret =
        readSectorsCHS(readBuffer, 1, cyl, head, sect, bpb->bootDrive);
    if (ret >> 8) {
        printf("Read error: %u\n", ret >> 8);
        halt();
    }
    printf("Read sectors: %u\n", ret & 0xff);

    for (int i = 0; i < 16; i++) {
        if (i)
            printf(" ");
        printf("%02x", readBuffer[i]);
    }
    printf("\n");
}

