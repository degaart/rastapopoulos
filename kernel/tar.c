#include "tar.h"
#include <stdint.h>
#include <string.h>

#define TAR_BLOCK_SIZE 512

static bool zero_block(const unsigned char* block)
{
    for (size_t i = 0; i < TAR_BLOCK_SIZE; ++i) {
        if (block[i])
            return false;
    }
    return true;
}

static bool octal(const unsigned char* field, size_t len, size_t* value)
{
    size_t result = 0;
    size_t i = 0;
    while (i < len && field[i] == ' ')
        ++i;
    for (; i < len && field[i] >= '0' && field[i] <= '7'; ++i) {
        size_t digit = field[i] - '0';
        if (result > (SIZE_MAX - digit) / 8)
            return false;
        result = result * 8 + digit;
    }
    for (; i < len; ++i) {
        if (field[i] != 0 && field[i] != ' ')
            return false;
    }
    *value = result;
    return true;
}

static bool read_header(const unsigned char* header, size_t available,
                        size_t* size, size_t* step)
{
    size_t checksum;
    size_t sum = 0;
    if (available < TAR_BLOCK_SIZE || !header[0] ||
        !octal(header + 148, 8, &checksum) || !octal(header + 124, 12, size))
        return false;
    for (size_t i = 0; i < TAR_BLOCK_SIZE; ++i)
        sum += i >= 148 && i < 156 ? ' ' : header[i];
    if (sum != checksum || *size > available - TAR_BLOCK_SIZE)
        return false;
    size_t blocks = *size / TAR_BLOCK_SIZE + (*size % TAR_BLOCK_SIZE != 0);
    if (blocks > (available - TAR_BLOCK_SIZE) / TAR_BLOCK_SIZE)
        return false;
    *step = TAR_BLOCK_SIZE + blocks * TAR_BLOCK_SIZE;
    return true;
}

static size_t entry_name(const unsigned char* header, char* name)
{
    size_t len = 0;
    if (!memcmp(header + 257, "ustar", 5)) {
        for (size_t i = 0; i < 155 && header[345 + i]; ++i)
            name[len++] = header[345 + i];
        if (len)
            name[len++] = '/';
    }
    for (size_t i = 0; i < 100 && header[i]; ++i)
        name[len++] = header[i];
    if (len >= 2 && name[0] == '.' && name[1] == '/') {
        memmove(name, name + 2, len - 2);
        len -= 2;
    }
    name[len] = 0;
    return len;
}

bool tar_open(struct Tar* tar, const void* buffer, size_t len)
{
    if (!tar)
        return false;
    tar->buffer = NULL;
    tar->len = 0;
    if (!buffer)
        return false;
    const unsigned char* bytes = buffer;
    size_t offset = 0;
    while (len - offset >= TAR_BLOCK_SIZE) {
        if (zero_block(bytes + offset)) {
            if (len - offset < 2 * TAR_BLOCK_SIZE ||
                !zero_block(bytes + offset + TAR_BLOCK_SIZE))
                return false;
            tar->buffer = buffer;
            tar->len = offset;
            return true;
        }
        size_t size;
        size_t step;
        if (!read_header(bytes + offset, len - offset, &size, &step))
            return false;
        offset += step;
    }
    return false;
}

bool tar_open_file(const struct Tar* tar, const char* filename,
                   void* const* buffer, size_t* len)
{
    if (!tar || !tar->buffer || !filename || !buffer || !len)
        return false;
    for (size_t offset = 0; offset < tar->len;) {
        const unsigned char* header = tar->buffer + offset;
        size_t size;
        size_t step;
        if (!read_header(header, tar->len - offset, &size, &step))
            return false;
        char name[257];
        entry_name(header, name);
        if (!strcmp(name, filename)) {
            if (header[156] != 0 && header[156] != '0')
                return false;
            void* data = (void*)(header + TAR_BLOCK_SIZE);
            /* The public API declares the output pointer const. */
            memcpy((void*)buffer, &data, sizeof(data));
            *len = size;
            return true;
        }
        offset += step;
    }
    return false;
}

bool tar_list(const struct Tar* tar,
              void (*callback)(const struct TarEntry*, void*), void* ctx)
{
    if (!tar || !tar->buffer || !callback)
        return false;
    for (size_t offset = 0; offset < tar->len;) {
        const unsigned char* header = tar->buffer + offset;
        size_t size;
        size_t step;
        if (!read_header(header, tar->len - offset, &size, &step))
            return false;
        char name[257];
        size_t name_len = entry_name(header, name);
        if (name_len >= TAR_ENTRY_NAME_MAX)
            return false;
        struct TarEntry entry = {0};
        memcpy(entry.name, name, name_len + 1);
        entry.is_dir = header[156] == '5';
        entry.size = size;
        callback(&entry, ctx);
        offset += step;
    }
    return true;
}
