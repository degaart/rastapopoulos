#include "../kernel/vfs.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t state;
static unsigned initial_seed;
static size_t case_number;
static unsigned random_number(void)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}
static bool reference_utf8(const unsigned char* p, size_t n)
{
    size_t i = 0;
    while (i < n) {
        unsigned c = p[i++], count, minimum;
        if (!c)
            return false;
        if (c < 128)
            continue;
        if (c >= 0xc0 && c <= 0xdf) {
            count = 1;
            minimum = 128;
            c &= 31;
        } else if (c >= 0xe0 && c <= 0xef) {
            count = 2;
            minimum = 2048;
            c &= 15;
        } else if (c >= 0xf0 && c <= 0xf7) {
            count = 3;
            minimum = 65536;
            c &= 7;
        } else
            return false;
        if (count > n - i)
            return false;
        while (count--) {
            unsigned b = p[i++];
            if ((b & 0xc0) != 0x80)
                return false;
            c = (c << 6) | (b & 63);
        }
        if (c < minimum || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
            return false;
    }
    return true;
}
static void check(bool ok, const char* reason, const unsigned char* p,
                  size_t n)
{
    if (ok)
        return;
    fprintf(stderr, "seed=%u case=%zu length=%zu: %s\n", initial_seed,
            case_number, n, reason);
    FILE* f = fopen(
        PATH_TEST ? "build/path_failure.bin" : "build/utf8_failure.bin", "wb");
    if (f) {
        if (n)
            fwrite(p, 1, n, f);
        fclose(f);
    }
    exit(1);
}
static void exercise(unsigned char* p, size_t n)
{
    unsigned char* saved = malloc(n ? n : 1);
    if (!saved)
        exit(2);
    memcpy(saved, p, n);
    struct StringSlice s = {(const char*)p, n};
    bool valid = reference_utf8(p, n);
    if (!PATH_TEST)
        check(utf8_validate(&s) == valid, "UTF-8 mismatch", p, n);
    else {
        struct ParsedPath out = {'Q', true, {"sentinel", 8}, true};
        struct ParsedPath old = out;
        size_t prefix = 0, first = n, last = 0;
        char drive = 0;
        for (size_t i = 0; i < n; i++) {
            if (p[i] != ':')
                continue;
            unsigned c = p[0];
            if (c >= 'a' && c <= 'z')
                c -= 32;
            if (i != 1 || c < 'A' || c > 'Z')
                valid = false;
            else {
                prefix = 2;
                drive = c;
            }
        }
        for (size_t i = prefix; i < n; i++)
            if (p[i] != '\\') {
                if (first == n)
                    first = i;
                last = i + 1;
            }
        bool ok = vfs_path_parse(&s, &out);
        check(ok == (valid && n != 0), "path acceptance", p, n);
        if (ok) {
            size_t offset = first == n ? n : first;
            check(out.drive == drive &&
                      out.start_at_root == (prefix < n && p[prefix] == '\\') &&
                      out.trailing_separator == (p[n - 1] == '\\'),
                  "path flags", p, n);
            check(out.components.buf == (const char*)p + offset &&
                      out.components.len == (first == n ? 0 : last - first),
                  "component slice", p, n);
        } else
            check(out.drive == old.drive &&
                      out.start_at_root == old.start_at_root &&
                      out.trailing_separator == old.trailing_separator &&
                      out.components.buf == old.components.buf &&
                      out.components.len == old.components.len,
                  "failure modified output", p, n);
    }
    check(!memcmp(saved, p, n), "modified input", p, n);
    free(saved);
    case_number++;
}
static void generated(size_t n, unsigned mode)
{
    unsigned char* p = malloc(n ? n : 1);
    if (!p)
        exit(2);
    const unsigned char pattern[] = {'A', 0xc3, 0xa9, 0xe6, 0x96, 0x87, '\\'};
    for (size_t i = 0; i < n; i++) {
        if (mode == 0)
            p[i] = 'A';
        else if (mode == 1)
            p[i] = '\\';
        else if (mode == 2)
            p[i] = i % 3 ? 'X' : '\\';
        else if (mode == 3)
            p[i] = pattern[i % sizeof(pattern)];
        else
            p[i] = random_number() & 255;
    }
    if (mode == 5 && n) {
        memset(p, 'A', n);
        p[n - 1] = ':';
    }
    if (mode == 6 && n) {
        memset(p, 'A', n);
        p[n - 1] = 0xe2;
    }
    exercise(p, n);
    free(p);
}
int main(int argc, char** argv)
{
    initial_seed = argc > 1 ? strtoul(argv[1], NULL, 0) : 305419896;
    state = initial_seed ? initial_seed : 1;
    size_t iterations = argc > 2 ? strtoull(argv[2], NULL, 0) : 100000;
    struct StringSlice empty = {NULL, 0}, bad = {NULL, 1};
    struct ParsedPath out;
    if (PATH_TEST) {
        check(!vfs_path_parse(NULL, &out) && !vfs_path_parse(&empty, &out) &&
                  !vfs_path_parse(&empty, NULL) && !vfs_path_parse(&bad, &out),
              "null arguments", NULL, 0);
    } else
        check(!utf8_validate(NULL) && utf8_validate(&empty) &&
                  !utf8_validate(&bad),
              "null arguments", NULL, 0);
    const char* cases[] = {"",
                           "A:",
                           "a:\\X\\\\Y\\",
                           "\\\\",
                           "..",
                           "1:X",
                           "AA:X",
                           "A::X",
                           "X\\B:Y",
                           "\xc0\x80",
                           "\xed\xa0\x80",
                           "\xf4\x90\x80\x80",
                           "\xf4\x8f\xbf\xbf"};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        exercise((unsigned char*)cases[i], strlen(cases[i]));
    for (size_t n = 0; n < 32; n++)
        for (unsigned m = 0; m < 7; m++)
            generated(n, m);
    for (size_t i = 0; i < iterations; i++)
        generated(random_number() % 4097, random_number() % 7);
    size_t sizes[] = {1u << 20, 16u << 20, 64u << 20};
    for (size_t i = 0; i < 3; i++)
        for (int delta = -1; delta <= 1; delta++)
            for (unsigned m = 0; m < 7; m++)
                generated(sizes[i] + delta, m);
    for (unsigned i = 0; i < 16; i++)
        generated((1u << 20) + random_number() % ((63u << 20) + 1),
                  random_number() % 7);
    printf("%s: %zu cases passed; seed=%u; large buffers through 64 MiB + 1\n",
           PATH_TEST ? "path" : "utf8", case_number, initial_seed);
    return 0;
}
