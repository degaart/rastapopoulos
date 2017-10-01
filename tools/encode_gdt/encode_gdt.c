#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

struct gdt_entry {
   uint16_t limit_low;          // The lower 16 bits of the limit.
   uint16_t base_low;           // The lower 16 bits of the base.
   uint8_t  base_middle;        // The next 8 bits of the base.
   uint8_t  access;             // Access flags, determine what ring this segment can be used in.
   uint8_t  granularity;        // Actually, also contains the bits 19:16 of segment limit
   uint8_t  base_high;          // The last 8 bits of the base.
} __attribute__((packed));

void create_descriptor(uint32_t base, uint32_t limit, uint16_t flag)
{
    union {
        uint8_t  b[8];
        uint16_t w[4];
        uint32_t d[2];
        uint64_t ll;
        struct gdt_entry e;
    } descriptor;
 
    // Create the high 32 bit segment
    descriptor.ll  =  limit       & 0x000F0000;         // set limit bits 19:16
    descriptor.ll |= (flag <<  8) & 0x00F0FF00;         // set type, p, dpl, s, g, d/b, l and avl fields
    descriptor.ll |= (base >> 16) & 0x000000FF;         // set base bits 23:16
    descriptor.ll |=  base        & 0xFF000000;         // set base bits 31:24
    
    // Shift by 32 to allow for low part of segment
    descriptor.ll <<= 32;
 
    // Create the low 32 bit segment
    descriptor.ll |= base  << 16;                       // set base bits 15:0
    descriptor.ll |= limit  & 0x0000FFFF;               // set limit bits 15:0
 
    printf("uint64_t: 0x%.16llX\n", descriptor.ll);

    printf("uint32_t: ");
    for(int i = 0; i < 2; i++) {
        if(i)
            printf(", ");
        printf("0x%08X", descriptor.d[i]);
    }
    printf("\n");

    printf("uint16_t: ");
    for(int i = 0; i < 4; i++) {
        if(i)
            printf(" ");
        printf("0x%04X", descriptor.w[i]);
    }
    printf("\n");

    printf("uint8_t: ");
    for(int i = 0; i < 8; i++) {
        if(i)
            printf(" ");
        printf("0x%02X", descriptor.b[i]);
    }
    printf("\n");
}

int main(int argc, char** argv)
{
    if(argc < 4) {
        fprintf(stderr, "Usage: %s <base> <limit> <flags>\n", argv[0]);
        return 1;
    }

    const char* sbase = argv[1];
    const char* slimit = argv[2];
    const char* sflags = argv[3];
    
    char* end;
    uint32_t base = strtoul(sbase, &end, 0);
    if(*end) {
        fprintf(stderr, "Invalid base: \"%s\"\n", sbase);
        return 1;
    }

    uint32_t limit = strtoul(slimit, &end, 0);
    if(*end) {
        fprintf(stderr, "Invalid limit: \"%s\"\n", slimit);
        return 1;
    }

    uint32_t flags = strtoul(sflags, &end, 0);
    if(*end) {
        fprintf(stderr, "Invalid flags: \"%s\"\n", sflags);
        return 1;
    } else if(flags & 0xFFFF0000) {
        fprintf(stderr, "Flags out of range: \"%s\"\n", sflags);
        return 1;
    }

    printf("base: 0x%08X, limit: 0x%08X, flags: 0x%04X\n",
           base, limit, flags);
    create_descriptor(base, limit, (uint16_t)flags);
    return 0;
}


