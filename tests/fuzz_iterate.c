#include "../kernel/vfs.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t state;
static unsigned random_byte(void)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}
int main(int argc, char** argv)
{
    unsigned seed = argc > 1 ? strtoul(argv[1], NULL, 0) : 305419896;
    state = seed ? seed : 1;
    size_t iterations = argc > 2 ? strtoull(argv[2], NULL, 0) : 100000;
    for (size_t c = 0; c < iterations + 4; c++) {
        size_t n =
            c < 4 ? (c == 3 ? (64u << 20) + 1 : c) : random_byte() % 4097;
        char* data = malloc(n ? n : 1);
        char* saved = malloc(n ? n : 1);
        if (!data || !saved)
            return 2;
        for (size_t i = 0; i < n; i++)
            data[i] = random_byte() % 3 ? random_byte() & 255 : '\\';
        memcpy(saved, data, n);
        struct StringSlice s = {data, n};
        size_t position = 0;
        bool ok = true;
        do {
            size_t start = position;
            while (start < n && saved[start] == '\\')
                start++;
            size_t end = start;
            while (end < n && saved[end] != '\\')
                end++;
            position = end;
            while (position < n && saved[position] == '\\')
                position++;
            struct StringSlice part = vfs_path_iterate(&s);
            ok = part.len == end - start &&
                 part.buf == (end == start ? NULL : data + start) &&
                 s.buf == data + position && s.len == n - position;
        } while (ok && position < n);
        ok = ok && vfs_path_iterate(&s).len == 0 &&
             vfs_path_iterate(&s).buf == NULL && !memcmp(saved, data, n);
        if (!ok) {
            fprintf(stderr, "seed=%u case=%zu: iterator mismatch\n", seed, c);
            FILE* f = fopen("build/iterate_failure.bin", "wb");
            if (f) {
                fwrite(saved, 1, n, f);
                fclose(f);
            }
            return 1;
        }
        free(saved);
        free(data);
    }
    puts("iterator fuzz tests passed");
}
