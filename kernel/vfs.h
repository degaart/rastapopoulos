#pragma once

#include "block_device.h"
#include "string_slice.h"
#include <stdbool.h>
#include <stddef.h>

#define MAX_PATH 4096

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
    StringSlice components;

    /* Whether a trailing separator was present */
    bool trailing_separator;
};

bool utf8_validate(StringSlice path);

bool vfs_init(void);
bool vfs_path_parse(StringSlice path, struct ParsedPath* parsed_path);
StringSlice vfs_path_basename(StringSlice path);

/*
 * Iterate the components of a ParsedPath::components.
 * Returns an empty StringSlice when at the end of the components
 * Advances the slice past the component and following backslashes.
 * NULL inputs return an empty slice unchanged. Exhaustion is stable.
 */
StringSlice vfs_path_iterate(StringSlice* buf);

struct VFSNode;

struct VFSMount
{
    struct FS* fs;
    struct VFSNode* (*lookup)(struct VFSMount*, const struct VFSNode*,
                              StringSlice);
};

struct VFSNode
{
    struct FS* fs;
    bool is_dir;
    StringSlice name;
    uint64_t size;

    void (*destroy)(struct VFSNode*);
};

struct FS
{
    char name[16];
    struct VFSMount* (*mount)(struct FS*, struct BlockDevice*);
    bool (*unmount)(struct FS*, struct VFSMount*);
};

struct VFSData
{
    char current_drive;
    char* current_dirs[26];
};

/*
 * drive: 'A' to 'Z'
 */
bool vfs_mount(char drive, struct FS* fs, struct BlockDevice* dev);
bool vfs_unmount(char drive);
struct VFSNode* vfs_lookup(const struct VFSData* vfs_data, StringSlice path);
void vfs_node_destroy(struct VFSNode* node);

struct FindData
{
    const char* filename;
    uint64_t size;
    bool is_dir;
};

void* vfs_find_first(StringSlice path, struct FindData* find_data);
bool vfs_find_next(void* handle, struct FindData* find_data);
void vfs_find_close(void* handle);

