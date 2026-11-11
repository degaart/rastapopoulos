#include "../kernel/vfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                              \
    do {                                                                      \
        if (!(x)) {                                                           \
            fprintf(stderr, "%d: %s\n", __LINE__, #x);                        \
            exit(1);                                                          \
        }                                                                     \
    } while (0)
int main(void)
{
    char data[] = {'\\', '\\', '.', '\\', '\\', '.', '.', '\\', 0, 'X', '\\'};
    char saved[sizeof(data)];
    memcpy(saved, data, sizeof(data));
    struct StringSlice s = {data, sizeof(data)};
    size_t starts[] = {2, 5, 8};
    size_t lengths[] = {1, 2, 2};
    size_t next[] = {5, 8, 11};
    for (size_t i = 0; i < 3; i++) {
        struct StringSlice part = vfs_path_iterate(&s);
        CHECK(part.buf == data + starts[i]);
        CHECK(part.len == lengths[i]);
        CHECK(s.buf == data + next[i]);
        CHECK(s.len == sizeof(data) - next[i]);
    }
    CHECK(vfs_path_iterate(&s).len == 0);
    CHECK(vfs_path_iterate(&s).buf == NULL);
    CHECK(!memcmp(data, saved, sizeof(data)));
    CHECK(vfs_path_iterate(NULL).len == 0);
    s = (struct StringSlice) {NULL, 3};
    CHECK(vfs_path_iterate(&s).len == 0);
    CHECK(s.buf == NULL && s.len == 3);
    puts("iterator unit tests passed");
}
