#pragma once

#include <stddef.h>

struct PSP
{
    uint8_t pid;
    char* args;
    size_t args_len;
} __attribute__((packed));

