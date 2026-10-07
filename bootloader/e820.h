#pragma once

#include <queue.h>
#include <stdbool.h>
#include <stdint.h>

#define E820_USABLE       1
#define E820_RESERVED     2
#define E820_ACPI_RECLAIM 3
#define E820_ACPI_NVS     4

struct E820Entry
{
    struct
    {
        uint32_t lo;
        uint32_t hi;
    } base;
    struct
    {
        uint32_t lo;
        uint32_t hi;
    } length;
    uint32_t type;

    STAILQ_ENTRY(E820Entry) entries;
} __attribute__((packed));

bool e820(uint32_t* cookie, void* buf, uint16_t len);

