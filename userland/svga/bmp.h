#pragma once

#include <stdint.h>
#include <stddef.h>

struct bmpfileheader {
    uint16_t signature;
    uint32_t size;
    uint16_t reserved[2];
    uint32_t offset;
} __attribute((packed));

struct bmpinfoheader {
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bpp;
    uint32_t compression;
    uint32_t img_size;
    int32_t horzres;
    int32_t vertres;
    uint32_t ncols;
    uint32_t icols;
} __attribute((packed));

