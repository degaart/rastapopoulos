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
        struct StringSlice slice = {valid[i], strlen(valid[i])};
        CHECK(utf8_validate(&slice));
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
        struct StringSlice slice = {invalid[i], strlen(invalid[i])};
        CHECK(!utf8_validate(&slice));
    }
    CHECK(!utf8_validate(NULL));
    struct StringSlice slice = {NULL, 0};
    CHECK(utf8_validate(&slice));
    slice.len = 1;
    CHECK(!utf8_validate(&slice));
    char bytes[] = {'A', 0, 'B', '\xff'};
    slice.buf = bytes;
    slice.len = 1;
    CHECK(utf8_validate(&slice));
    slice.len = 3;
    CHECK(!utf8_validate(&slice));
    slice.buf = bytes + 1;
    slice.len = 1;
    CHECK(!utf8_validate(&slice));
}

static void test_path_parsing(void)
{
    trace("Testing path parsing");
    CHECK(vfs_path_iterate(NULL).len == 0);
    struct StringSlice absent = {NULL, 1};
    CHECK(vfs_path_iterate(&absent).len == 0);
    CHECK(absent.buf == NULL && absent.len == 1);
    char raw[] = {'X', 0, 'Y', '\\'};
    struct StringSlice cursor = {raw, sizeof(raw)};
    struct StringSlice part = vfs_path_iterate(&cursor);
    CHECK(part.buf == raw && part.len == 3);
    CHECK(cursor.buf == raw + sizeof(raw) && cursor.len == 0);
    CHECK(vfs_path_iterate(&cursor).len == 0);

    struct ParsedPath sentinel = {'Q', true, {"sentinel", 8}, true};
    struct ParsedPath output = sentinel;
    struct StringSlice empty = {NULL, 0};
    CHECK(!vfs_path_parse(NULL, &output));
    CHECK(!vfs_path_parse(&empty, &output));
    CHECK(!vfs_path_parse(&empty, NULL));
    char embedded_nul[] = {'A', 0, 'B'};
    struct StringSlice invalid_slice = {embedded_nul, sizeof(embedded_nul)};
    CHECK(!vfs_path_parse(&invalid_slice, &output));
    CHECK(output.drive == sentinel.drive);
    CHECK(output.start_at_root == sentinel.start_at_root);
    CHECK(output.components.buf == sentinel.components.buf);
    CHECK(output.components.len == sentinel.components.len);
    CHECK(output.trailing_separator == sentinel.trailing_separator);
    struct StringSlice exact = {"a:\\\\ONE\\\\TWO\\\\", 0};
    exact.len = strlen(exact.buf);
    CHECK(vfs_path_parse(&exact, &output));
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
        struct StringSlice input = {cases[i].input, strlen(cases[i].input)};
        struct ParsedPath parsed;
        CHECK(utf8_validate(&input));
        CHECK(vfs_path_parse(&input, &parsed));
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

        struct StringSlice remaining = parsed.components;
        for (size_t j = 0; cases[i].components[j]; j++) {
            struct StringSlice component = vfs_path_iterate(&remaining);
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
        struct StringSlice input = {invalid[i], strlen(invalid[i])};
        struct ParsedPath parsed;
        CHECK(!vfs_path_parse(&input, &parsed));
    }

    char bounded[] = {'c', ':', '\\', 'X', '\\', 'Y', '\xff'};
    struct StringSlice input = {bounded, 6};
    struct ParsedPath parsed;
    CHECK(utf8_validate(&input));
    CHECK(vfs_path_parse(&input, &parsed));
    CHECK(parsed.drive == 'C');
    CHECK(parsed.start_at_root);
    CHECK(!parsed.trailing_separator);
    CHECK(parsed.components.len == 3);
    CHECK(!memcmp(parsed.components.buf, "X\\Y", 3));
}

void test_vfs(void)
{
    trace("*** VFS tests started");
    test_utf8_validation();
    test_path_parsing();
    trace("*** VFS tests done");
}

