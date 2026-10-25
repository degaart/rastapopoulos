#pragma once

#include <stdbool.h>
#include <stddef.h>

#define TAR_ENTRY_NAME_MAX 64
struct TarEntry
{
    char name[TAR_ENTRY_NAME_MAX];
    bool is_dir;
    size_t size;
};

struct Tar
{
    const unsigned char* buffer;
    size_t len;
};

bool tar_open(struct Tar* tar, const void* buffer, size_t len);
bool tar_open_file(const struct Tar* tar, const char* filename,
                   void* const* buffer, size_t* len);
bool tar_list(const struct Tar* tar,
              void (*callback)(const struct TarEntry*, void*), void* ctx);

