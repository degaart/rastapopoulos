#include "../kmalloc.h"
#include "../ramdisk.h"
#include "../tar.h"
#include "../vfs.h"
#include "test.h"
#include <string.h>

static void test_utf8_validation(void)
{
    trace("Testing utf8 validation");

    const char* valid[] = {
        "",
        "ASCII",
        "\x01\x7f",
        "\xc2\x80",
        "\xdf\xbf",
        "\xe0\xa0\x80",
        "\xed\x9f\xbf",
        "\xee\x80\x80",
        "\xef\xbf\xbf",
        "\xf0\x90\x80\x80",
        "\xf4\x8f\xbf\xbf",
        "caf\xc3\xa9\\\xe6\x96\x87\xf0\x9f\x98\x80",
    };
    for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); i++) {
        StringSlice slice = SLICEPTR(valid[i]);
        CHECK(utf8_validate(slice));
    }
    const char* invalid[] = {
        "\x80",
        "\xbf",
        "\xc0\x80",
        "\xc1\xbf",
        "\xc2",
        "\xc2"
        "A",
        "\xe0\x9f\xbf",
        "\xed\xa0\x80",
        "\xe2\x82",
        "\xe2\x28\xa1",
        "\xf0\x8f\xbf\xbf",
        "\xf4\x90\x80\x80",
        "\xf5\x80\x80\x80",
        "\xff",
        "\xf0\x90\x80",
        "\xf0\x90\x80"
        "A",
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        StringSlice slice = SLICEPTR(invalid[i]);
        CHECK(!utf8_validate(slice));
    }
    CHECK(utf8_validate(NULLSLICE));
    StringSlice slice = {0};
    CHECK(utf8_validate(slice));
    slice.len = 1;
    CHECK(!utf8_validate(slice));
    char bytes[] = {'A', 0, 'B', '\xff'};
    slice.buf = bytes;
    slice.len = 1;
    CHECK(utf8_validate(slice));
    slice.len = 3;
    CHECK(!utf8_validate(slice));
    slice.buf = bytes + 1;
    slice.len = 1;
    CHECK(!utf8_validate(slice));
}

static void test_path_parsing(void)
{
    trace("Testing path parsing");
    CHECK(vfs_path_iterate(NULL).len == 0);
    StringSlice absent = {NULL, 1};
    CHECK(vfs_path_iterate(&absent).len == 0);
    CHECK(absent.buf == NULL && absent.len == 1);
    char raw[] = {'X', 0, 'Y', '\\'};
    StringSlice cursor = {raw, sizeof(raw)};
    StringSlice part = vfs_path_iterate(&cursor);
    CHECK(part.buf == raw && part.len == 3);
    CHECK(cursor.buf == raw + sizeof(raw) && cursor.len == 0);
    CHECK(vfs_path_iterate(&cursor).len == 0);

    struct ParsedPath sentinel = {'Q', true, {"sentinel", 8}, true};
    struct ParsedPath output = sentinel;
    StringSlice empty = {0};
    CHECK(!vfs_path_parse(NULLSLICE, &output));
    CHECK(!vfs_path_parse(empty, &output));
    CHECK(!vfs_path_parse(empty, NULL));
    char embedded_nul[] = {'A', 0, 'B'};
    StringSlice invalid_slice = {embedded_nul, sizeof(embedded_nul)};
    CHECK(!vfs_path_parse(invalid_slice, &output));
    CHECK(output.drive == sentinel.drive);
    CHECK(output.start_at_root == sentinel.start_at_root);
    CHECK(output.components.buf == sentinel.components.buf);
    CHECK(output.components.len == sentinel.components.len);
    CHECK(output.trailing_separator == sentinel.trailing_separator);
    StringSlice exact = {"a:\\\\ONE\\\\TWO\\\\", 0};
    exact.len = strlen(exact.buf);
    CHECK(vfs_path_parse(exact, &output));
    CHECK(output.components.len == strlen("ONE\\\\TWO"));
    CHECK(!memcmp(output.components.buf, "ONE\\\\TWO", output.components.len));

    struct
    {
        const char* input;
        char drive;
        bool rooted;
        bool trailing;
        const char* components[6];
    } cases[] = {
        {"FILE.TXT", 0, false, false, {"FILE.TXT"}},
        {"\\FILE.TXT", 0, true, false, {"FILE.TXT"}},
        {"A:FILE.TXT", 'A', false, false, {"FILE.TXT"}},
        {"a:\\DOCS\\README.TXT", 'A', true, false, {"DOCS", "README.TXT"}},
        {"z:DOCS\\README.TXT", 'Z', false, false, {"DOCS", "README.TXT"}},
        {"A:", 'A', false, false, {NULL}},
        {"A:\\", 'A', true, true, {NULL}},
        {"\\", 0, true, true, {NULL}},
        {"\\\\\\", 0, true, true, {NULL}},
        {"DOCS\\", 0, false, true, {"DOCS"}},
        {"DOCS\\\\\\", 0, false, true, {"DOCS"}},
        {"b:\\\\DOCS\\\\OLD\\\\", 'B', true, true, {"DOCS", "OLD"}},
        {"\\\\FILE.TXT", 0, true, false, {"FILE.TXT"}},
        {".", 0, false, false, {"."}},
        {"..", 0, false, false, {".."}},
        {"DOCS\\\\.\\OLD\\..\\README.TXT",
         0,
         false,
         false,
         {"DOCS", ".", "OLD", "..", "README.TXT"}},
        {"C:\\caf\xc3\xa9\\\xe6\x96\x87\xe4\xbb\xb6",
         'C',
         true,
         false,
         {"caf\xc3\xa9", "\xe6\x96\x87\xe4\xbb\xb6"}},
        {"\xf0\x9f\x98\x80", 0, false, false, {"\xf0\x9f\x98\x80"}},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        trace("Path parsing case %u", (unsigned)i);
        StringSlice input = SLICEPTR(cases[i].input);
        struct ParsedPath parsed;
        CHECK(utf8_validate(input));
        CHECK(vfs_path_parse(input, &parsed));
        CHECK(parsed.drive == cases[i].drive);
        CHECK(parsed.start_at_root == cases[i].rooted);
        CHECK(parsed.trailing_separator == cases[i].trailing);
        CHECK(parsed.components.len <= input.len);
        if (parsed.components.len) {
            CHECK(parsed.components.buf >= input.buf);
            CHECK(parsed.components.buf <= input.buf + input.len);
            CHECK(parsed.components.len <=
                  (size_t)(input.buf + input.len - parsed.components.buf));
            CHECK(parsed.components.buf[parsed.components.len - 1] != '\\');
        }

        StringSlice remaining = parsed.components;
        for (size_t j = 0; cases[i].components[j]; j++) {
            StringSlice component = vfs_path_iterate(&remaining);
            size_t expected_len = strlen(cases[i].components[j]);
            CHECK(component.len == expected_len);
            CHECK(component.buf != NULL);
            CHECK(
                !memcmp(component.buf, cases[i].components[j], expected_len));
        }
        CHECK(vfs_path_iterate(&remaining).len == 0);
        CHECK(vfs_path_iterate(&remaining).len == 0);
    }

    const char* invalid[] = {
        "",
        "1:FILE",
        ":FILE",
        "AA:FILE",
        "A::FILE",
        "DIR\\B:FILE",
        "\x80",
        "\xc0\xaf",
        "\xc2",
        "\xe2\x82",
        "\xe2\x28\xa1",
        "\xed\xa0\x80",
        "\xf0\x80\x80\xaf",
        "\xf4\x90\x80\x80",
        "\xf5\x80\x80\x80",
        "A:\\DOCS\\\xff",
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        StringSlice input = SLICEPTR(invalid[i]);
        struct ParsedPath parsed;
        CHECK(!vfs_path_parse(input, &parsed));
    }

    char bounded[] = {'c', ':', '\\', 'X', '\\', 'Y', '\xff'};
    StringSlice input = {bounded, 6};
    struct ParsedPath parsed;
    CHECK(utf8_validate(input));
    CHECK(vfs_path_parse(input, &parsed));
    CHECK(parsed.drive == 'C');
    CHECK(parsed.start_at_root);
    CHECK(!parsed.trailing_separator);
    CHECK(parsed.components.len == 3);
    CHECK(!memcmp(parsed.components.buf, "X\\Y", 3));
}

struct MemBlockDevice
{
    struct BlockDevice dev;
    const void* buf;
    size_t len;
};

bool mem_check_media(struct BlockDevice* dev, unsigned* generation)
{
    *generation = 0;
    return true;
}

bool mem_read(struct BlockDevice* dev, uint64_t lba, char* buffer,
              size_t buffer_len)
{
    if (!dev || !buffer || buffer_len != 512)
        return false;

    struct MemBlockDevice* memdev = (struct MemBlockDevice*)dev;
    if (lba * 512 >= memdev->len)
        return false;

    if ((lba * 512) + buffer_len > memdev->len)
        buffer_len = memdev->len - (lba * 512);
    memcpy(buffer, memdev->buf + (lba * 512), buffer_len);
    return true;
}

bool mem_write(struct BlockDevice* dev, uint64_t lba, const char* buffer,
               size_t buffer_len)
{
    return false;
}

struct TarFS
{
    struct FS fs;
};

struct TarVFSMount
{
    struct VFSMount mount;
    struct Tar tar;
};

struct TarVFSNode
{
    struct VFSNode node;
    struct Tar* tar;
    char* path;
    struct TarEntry entry;
};

struct LookupContext
{
    const char* path;
    struct TarEntry entry;
    bool found;
};

/* Strips leading and trailing separators */
static void strip(StringSlice* s, char separator)
{
    while (s->len && (*s->buf == '.' || *s->buf == separator))
        s->buf++;
    while (s->len && (s->buf[s->len - 1] == separator))
        s->len--;
}

static void lookup_callback(const struct TarEntry* entry, void* lookupctx)
{
    struct LookupContext* ctx = lookupctx;

    StringSlice entryname = SLICEPTR(entry->name);
    strip(&entryname, '/');

    StringSlice path = SLICEPTR(ctx->path);
    strip(&path, '\\');

    if (string_slice_equals(path, entryname)) {
        ctx->found = true;
        ctx->entry = *entry;
    }
}

static bool lookup_entry(struct Tar* tar, const char* path,
                         struct TarEntry* entry)
{
    if (!tar || !entry)
        return false;

    struct LookupContext ctx = {
        .path = path,
        .found = false,
    };
    if (!tar_list(tar, lookup_callback, &ctx))
        return false;
    if (!ctx.found)
        return false;
    *entry = ctx.entry;
    return true;
}

static void tar_node_destroy(struct VFSNode* vfsnode)
{
    struct TarVFSNode* node = (struct TarVFSNode*)vfsnode;
    if (!node)
        return;
    kfree(node->path);
    kfree(node);
}

static struct VFSNode* tar_lookup(struct VFSMount* vfsmount,
                                  const struct VFSNode* vfsparent,
                                  StringSlice path)
{
    struct TarVFSMount* mount = (struct TarVFSMount*)vfsmount;
    if (!mount)
        return NULL;

    const char* parent_path = "";
    struct TarVFSNode* parent = (struct TarVFSNode*)vfsparent;
    if (parent) {
        if (!parent->entry.is_dir)
            return NULL;
        parent_path = parent->path;
    }

    if (!path.buf || !path.len) {
        if (parent)
            return NULL;

        struct TarEntry entry;
        if (!lookup_entry(&mount->tar, "", &entry))
            return NULL;

        struct TarVFSNode* node = knew(struct TarVFSNode);
        if (!node)
            return NULL;
        node->node.fs = mount->mount.fs;
        node->node.destroy = tar_node_destroy;
        node->tar = &mount->tar;
        node->path = strdup("");
        node->entry = entry;
        return (struct VFSNode*)node;
    }

    size_t namebuf_len = strlen(parent_path) + 1 + path.len + 1;
    char* namebuf = kmalloc(namebuf_len);
    if (*parent_path)
        snprintf(namebuf, namebuf_len, "%s/%.*s", parent_path, (int)path.len,
                 path.buf);
    else
        snprintf(namebuf, namebuf_len, "%.*s", (int)path.len, path.buf);

    struct TarEntry entry;
    if (!lookup_entry(&mount->tar, namebuf, &entry)) {
        kfree(namebuf);
        return NULL;
    }

    struct TarVFSNode* node = knew(struct TarVFSNode);
    if (!node) {
        kfree(namebuf);
        return NULL;
    }
    node->node.fs = mount->mount.fs;
    node->node.destroy = tar_node_destroy;
    node->tar = &mount->tar;
    node->path = namebuf;
    node->entry = entry;
    return (struct VFSNode*)node;
}

static struct VFSMount* tar_mount(struct FS* fs, struct BlockDevice* dev)
{
    struct MemBlockDevice* memdev = (struct MemBlockDevice*)dev;

    struct Tar tar;
    if (!tar_open(&tar, memdev->buf, memdev->len))
        return NULL;

    struct TarVFSMount* result = kmalloc(sizeof(struct TarVFSMount));
    if (!result)
        return NULL;
    memset(result, 0, sizeof(*result));
    result->mount.fs = fs;
    result->mount.lookup = tar_lookup;
    result->tar = tar;
    return (struct VFSMount*)result;
}

static bool tar_unmount(struct FS* fs, struct VFSMount* mount)
{
    if (!fs || !mount)
        return false;
    kfree(mount);
    return true;
}

static void test_lookup_entry(void)
{
    trace("Testing lookup_entry");

    struct Tar tar;
    CHECK(tar_open(&tar, ramdisk_start, ramdisk_end - ramdisk_start));

    struct TarEntry entry;
    CHECK(lookup_entry(&tar, "", &entry));
    trace("entry: name=%s, isdir=%s, size=%zu", entry.name,
          entry.is_dir ? "true" : "false", entry.size);
}

static void test_mount(void)
{
    trace("Testing vfs_mount");

    struct MemBlockDevice memdev;
    memdev.dev.check_media = mem_check_media;
    memdev.dev.read = mem_read;
    memdev.dev.write = mem_write;
    memdev.buf = ramdisk_start;
    memdev.len = ramdisk_end - ramdisk_start;

    struct TarFS tarfs;
    memset(&tarfs, 0, sizeof(tarfs));
    strlcpy(tarfs.fs.name, "TAR", sizeof(tarfs.fs.name));
    tarfs.fs.mount = tar_mount;
    tarfs.fs.unmount = tar_unmount;

    CHECK(!vfs_mount('a', (struct FS*)&tarfs, (struct BlockDevice*)&memdev));
    CHECK(!vfs_mount('A', NULL, (struct BlockDevice*)&memdev));
    CHECK(!vfs_mount('A', (struct FS*)&tarfs, NULL));

    CHECK(vfs_mount('A', (struct FS*)&tarfs, (struct BlockDevice*)&memdev));
    CHECK(vfs_unmount('A'));

    CHECK(vfs_mount('A', (struct FS*)&tarfs, (struct BlockDevice*)&memdev));
    CHECK(!vfs_mount('A', (struct FS*)&tarfs, (struct BlockDevice*)&memdev));
    CHECK(vfs_unmount('A'));

    struct VFSCurrentDirectories current_dirs;
    memset(&current_dirs, 0, sizeof(current_dirs));
    current_dirs.current_dirs[2] = "\\arf";

    CHECK(vfs_mount('C', (struct FS*)&tarfs, (struct BlockDevice*)&memdev));

    struct VFSNode* root = vfs_lookup('C', &current_dirs, SLICE("\\"));
    CHECK(root != NULL);

    struct VFSNode* seiche =
        vfs_lookup('C', &current_dirs, SLICE("\\arf\\gouda\\seiche"));
    CHECK(seiche != NULL);
    vfs_node_destroy(seiche);

    seiche = vfs_lookup('C', &current_dirs, SLICE("gouda\\seiche"));
    CHECK(seiche != NULL);
    vfs_node_destroy(seiche);

    current_dirs.current_dirs[2] = "arf\\";
    seiche = vfs_lookup('C', &current_dirs, SLICE("gouda\\seiche"));
    CHECK(seiche != NULL);
    vfs_node_destroy(seiche);

    current_dirs.current_dirs[2] = "arf";
    seiche = vfs_lookup('C', &current_dirs, SLICE("gouda\\seiche"));
    CHECK(seiche != NULL);
    vfs_node_destroy(seiche);

    current_dirs.current_dirs[2] = "arf\\gouda";
    seiche = vfs_lookup('C', &current_dirs, SLICE("seiche"));
    CHECK(seiche != NULL);
    vfs_node_destroy(seiche);

    current_dirs.current_dirs[2] = "";
    seiche = vfs_lookup('C', &current_dirs, SLICE("arf\\gouda\\seiche"));
    CHECK(seiche != NULL);
    vfs_node_destroy(seiche);

    vfs_node_destroy(root);
    CHECK(vfs_unmount('C'));
}

void test_vfs(void)
{
    trace("*** VFS tests started");
    test_utf8_validation();
    test_path_parsing();
    test_lookup_entry();
    test_mount();
    trace("*** VFS tests done");
}

