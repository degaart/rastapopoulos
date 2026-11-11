#pragma once

#include <stdbool.h>
#include <stddef.h>

struct StringSlice
{
    const char* buf;
    size_t len;
};

struct ParsedPath
{
    /* 0: invalid/absent drive, else, an uppercase char 'A' - 'Z' identifying
     * the drive */
    char drive;

    /* Whether the path starts at the root */
    bool start_at_root;

    /*
     * The rest of the path, minus the prefix (drive, colon, first backslash).
     * Also does not include the trailing separator, if present
     */
    struct StringSlice components;

    /* Whether a trailing separator was present */
    bool trailing_separator;
};

bool utf8_validate(const struct StringSlice* path);
bool vfs_path_parse(const struct StringSlice* path,
                    struct ParsedPath* parsed_path);

/*
 * Iterate the components of a ParsedPath::components.
 * Returns an empty StringSlice when at the end of the components
 * Advances the slice past the component and following backslashes.
 * NULL inputs return an empty slice unchanged. Exhaustion is stable.
 */
struct StringSlice vfs_path_iterate(struct StringSlice* buf);

