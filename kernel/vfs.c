#include "vfs.h"
#include "kernel.h"
#include "kmalloc.h"
#include "mutex.h"
#include "trace.h"
#include <string.h>

static struct VFSMount* mounts[26];
static struct Mutex lock;

bool utf8_validate(StringSlice path)
{
    if (!path.buf && path.len)
        return false;

    const unsigned char* bytes = (const unsigned char*)path.buf;
    size_t i = 0;
    while (i < path.len) {
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
        if (extra > path.len - i)
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

bool vfs_path_parse(StringSlice path, struct ParsedPath* parsed_path)
{
    if (!parsed_path || !path.len || !utf8_validate(path))
        return false;

    struct ParsedPath result = {0};
    size_t begin = 0;
    size_t end = path.len;
    if (end >= 2 && path.buf[1] == ':') {
        char drive = path.buf[0];
        if (drive >= 'a' && drive <= 'z')
            drive -= 'a' - 'A';
        if (drive < 'A' || drive > 'Z')
            return false;
        result.drive = drive;
        begin = 2;
    }

    for (size_t i = begin; i < end; i++) {
        if (path.buf[i] == ':')
            return false;
    }

    result.start_at_root = begin < end && path.buf[begin] == '\\';
    result.trailing_separator = path.buf[end - 1] == '\\';
    while (begin < end && path.buf[begin] == '\\')
        begin++;

    while (end > begin && path.buf[end - 1] == '\\')
        end--;

    result.components.buf = path.buf + begin;
    result.components.len = end - begin;
    *parsed_path = result;
    return true;
}

StringSlice vfs_path_iterate(StringSlice* buf)
{
    struct StringSlice result = {0};
    if (!buf || !buf->buf)
        return result;

    size_t start = 0;
    while (start < buf->len && buf->buf[start] == '\\')
        start++;

    size_t end = start;
    while (end < buf->len && buf->buf[end] != '\\')
        end++;

    if (end > start) {
        result.buf = buf->buf + start;
        result.len = end - start;
    }

    size_t next = end;
    while (next < buf->len && buf->buf[next] == '\\')
        next++;

    buf->buf += next;
    buf->len -= next;
    return result;
}

bool vfs_init(void)
{
    mutex_init(&lock);
    return true;
}

bool vfs_mount(char drive, struct FS* fs, struct BlockDevice* dev)
{
    if (drive < 'A' || drive > 'Z' || !fs || !dev)
        return false;
    if (!mutex_lock(&lock))
        return false;
    if (!fs->mount)
        return false;

    int idx = drive - 'A';
    if (mounts[idx]) {
        mutex_unlock(&lock);
        return false;
    }
    mounts[idx] = fs->mount(fs, dev);
    if (!mounts[idx]->fs)
        panic("Invalid mount returned from fs %s", fs->name);
    bool result = mounts[idx] != NULL;
    mutex_unlock(&lock);

    if (result)
        trace("Mounted drive %c: (%s)", drive, fs->name);
    else
        trace("Failed to mount drive %c: (%s)", drive, fs->name);
    return result;
}

bool vfs_unmount(char drive)
{
    if (drive < 'A' || drive > 'Z')
        return false;
    if (!mutex_lock(&lock))
        return false;

    int idx = drive - 'A';
    if (!mounts[idx]) {
        mutex_unlock(&lock);
        return false;
    }
    if (!mounts[idx]->fs)
        panic("Drive %c: has invalid fs pointer", drive);
    if (!mounts[idx]->fs->unmount) {
        mutex_unlock(&lock);
        return false;
    }

    bool result = mounts[idx]->fs->unmount(mounts[idx]->fs, mounts[idx]);
    if (result)
        mounts[idx] = NULL;
    mutex_unlock(&lock);

    if (result)
        trace("Unmounted drive %c:", drive);
    else
        trace("Failed to unmount drive %c:", drive);
    return result;
}

static struct VFSNode* lookup(struct VFSMount* mount,
                              const struct VFSNode* parent, StringSlice path)
{
    if (!mount)
        return NULL;

    struct VFSNode* node;
    if (!parent) {
        node = mount->lookup(mount, NULL, NULLSLICE); /* root dir */
        if (!node)
            return NULL;
    } else {
        node = (struct VFSNode*)parent;
    }

    if (!path.buf || !path.len)
        return node;

    StringSlice cookie = path;
    while (true) {
        StringSlice name = vfs_path_iterate(&cookie);
        if (!name.len)
            break;
        struct VFSNode* child = mount->lookup(mount, node, name);
        if (node != parent)
            node->destroy(node);

        node = child;
        if (!node)
            break;
    }

    return node;
}

struct VFSNode* vfs_lookup(char current_drive,
                           struct VFSCurrentDirectories* current_dirs,
                           StringSlice path)
{
    struct ParsedPath parsed_path;
    if (!vfs_path_parse(path, &parsed_path))
        return NULL;

    if (!parsed_path.drive) /* resolve drive */
        parsed_path.drive = current_drive;

    if (!mutex_lock(&lock))
        return NULL;

    struct VFSMount* mount = mounts[parsed_path.drive - 'A'];
    if (!mount) {
        mutex_unlock(&lock);
        return NULL;
    }

    struct VFSNode* parent = NULL;
    if (!parsed_path.start_at_root) { /* relative path */
        if (current_dirs->current_dirs[parsed_path.drive -
                                       'A']) { /* drive has current dir */
            StringSlice parent_slice =
                SLICEPTR(current_dirs->current_dirs[parsed_path.drive - 'A']);
            parent = lookup(mount, NULL, parent_slice);
        } else { /* start lookup at root */
            parent = lookup(mount, NULL, NULLSLICE);
        }

        if (!parent) {
            mutex_unlock(&lock);
            return NULL;
        }
    }

    struct VFSNode* node = lookup(mount, parent, parsed_path.components);
    vfs_node_destroy(parent);
    mutex_unlock(&lock);
    return node;
}

void vfs_node_destroy(struct VFSNode* node)
{
    if (!node)
        return;
    node->destroy(node);
}

