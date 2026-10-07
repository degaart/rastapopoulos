#pragma once

#include <stdint.h>

#define GDT_ACCESS_ACCESSED        0x01
#define GDT_ACCESS_DATA_WRITABLE   0x02
#define GDT_ACCESS_CODE_READABLE   0x02
#define GDT_ACCESS_DATA_DIRECTION  0x04
#define GDT_ACCESS_CODE_CONFORMING 0x04
#define GDT_ACCESS_EXECUTABLE      0x08
#define GDT_ACCESS_DESCRIPTOR      0x10
#define GDT_ACCESS_RING0           0x00
#define GDT_ACCESS_RING1           0x20
#define GDT_ACCESS_RING2           0x40
#define GDT_ACCESS_RING3           0x60
#define GDT_ACCESS_PRESENT         0x80

#define GDT_FLAG_LONG_MODE      0x20
#define GDT_FLAG_32BIT          0x40
#define GDT_FLAG_GRANULARITY_4K 0x80

#define GDT_ACCESS_CODE                                                       \
    (GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_DESCRIPTOR |          \
     GDT_ACCESS_EXECUTABLE | GDT_ACCESS_CODE_READABLE)

#define GDT_ACCESS_DATA                                                       \
    (GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_DESCRIPTOR |          \
     GDT_ACCESS_DATA_WRITABLE)

#define GDT_ACCESS_USERCODE                                                   \
    (GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_DESCRIPTOR |          \
     GDT_ACCESS_EXECUTABLE | GDT_ACCESS_CODE_READABLE)

#define GDT_ACCESS_USERDATA                                                   \
    (GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_DESCRIPTOR |          \
     GDT_ACCESS_DATA_WRITABLE)

#define GDT_FLAGS_32BIT_4K (GDT_FLAG_32BIT | GDT_FLAG_GRANULARITY_4K)

struct GdtEntry
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t limit_high_flags;
    uint8_t base_high;
} __attribute__((packed));

struct Gdtr
{
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct Gdt
{
    struct GdtEntry null;
    struct GdtEntry code;
    struct GdtEntry data;
    struct GdtEntry usercode;
    struct GdtEntry userdata;
} __attribute__((packed));

void gdt_set_entry(struct GdtEntry* entry, uint32_t base, uint32_t limit,
                   uint8_t access, uint8_t flags);
void gdt_load(const struct Gdtr* gdtr);

