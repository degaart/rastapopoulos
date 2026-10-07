#include "gdt.h"

void gdt_set_entry(struct GdtEntry* entry, uint32_t base, uint32_t limit,
                   uint8_t access, uint8_t flags)
{
    entry->limit_low = limit & 0xffff;
    entry->base_low = base & 0xffff;
    entry->base_mid = (base >> 16) & 0xff;
    entry->access = access;

    entry->limit_high_flags = ((limit >> 16) & 0x0f) | (flags & 0xf0);

    entry->base_high = (base >> 24) & 0xff;
}

