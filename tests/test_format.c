#include "format.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

struct Buffer
{
    char* ptr;
    size_t rem;
};

static void write_char(void* data, char ch)
{
    struct Buffer* buf = data;
    if (!buf->rem)
    {
        return;
    }
    *buf->ptr = ch;
    buf->ptr++;
    buf->rem--;
}

int main()
{
    char str[128];
    struct Buffer buffer = { str, sizeof(str) - 1 };
    format(write_char, &buffer, "%lu", 0xDEADBEEFul);
    *buffer.ptr = '\0';
    printf("%s\n", str);
    assert(!strcmp(str, "3735928559"));
    return 0;
}
