#include "vfs.h"

bool utf8_validate(const struct StringSlice* path)
{
    if (!path || (!path->buf && path->len))
        return false;

    const unsigned char* bytes = (const unsigned char*)path->buf;
    size_t i = 0;
    while (i < path->len) {
        unsigned char first = bytes[i++];
        if (!first)
            return false;
        if (first < 0x80)
            continue;

        size_t extra;
        unsigned char minimum = 0x80;
        unsigned char maximum = 0xbf;
        if (first >= 0xc2 && first <= 0xdf) {
            extra = 1;
        } else if (first >= 0xe0 && first <= 0xef) {
            extra = 2;
            if (first == 0xe0)
                minimum = 0xa0;
            if (first == 0xed)
                maximum = 0x9f;
        } else if (first >= 0xf0 && first <= 0xf4) {
            extra = 3;
            if (first == 0xf0)
                minimum = 0x90;
            if (first == 0xf4)
                maximum = 0x8f;
        } else {
            return false;
        }
        if (extra > path->len - i)
            return false;
        if (bytes[i] < minimum || bytes[i] > maximum)
            return false;
        i++;
        for (size_t j = 1; j < extra; j++, i++) {
            if (bytes[i] < 0x80 || bytes[i] > 0xbf)
                return false;
        }
    }
    return true;
}

bool vfs_path_parse(const struct StringSlice* path,
                    struct ParsedPath* parsed_path)
{
    if (!parsed_path || !path || !path->len || !utf8_validate(path))
        return false;

    struct ParsedPath result = {0};
    size_t begin = 0;
    size_t end = path->len;
    if (end >= 2 && path->buf[1] == ':') {
        char drive = path->buf[0];
        if (drive >= 'a' && drive <= 'z')
            drive -= 'a' - 'A';
        if (drive < 'A' || drive > 'Z')
            return false;
        result.drive = drive;
        begin = 2;
    }
    for (size_t i = begin; i < end; i++) {
        if (path->buf[i] == ':')
            return false;
    }
    result.start_at_root = begin < end && path->buf[begin] == '\\';
    result.trailing_separator = path->buf[end - 1] == '\\';
    while (begin < end && path->buf[begin] == '\\')
        begin++;
    while (end > begin && path->buf[end - 1] == '\\')
        end--;
    result.components.buf = path->buf + begin;
    result.components.len = end - begin;
    *parsed_path = result;
    return true;
}

struct StringSlice vfs_path_iterate(struct StringSlice* buf)
{
    struct StringSlice result = {.buf = NULL, .len = 0};
    return result;
}

