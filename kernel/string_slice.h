#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct StringSlice
{
    const char* buf;
    size_t len;
} StringSlice;

/* Create a StringSlice from a string literal */
#define S(s) (StringSlice) {.buf = (s), .len = sizeof(s) - 1}

/* Create a StringSlice from a char pointer */
#define SPTR(s)                                                               \
    (StringSlice)                                                             \
    {                                                                         \
        .buf = (s), .len = strlen(s)                                          \
    }

/* Create an empty string slice */
#define NULLSLICE (StringSlice) {.buf = NULL, .len = 0}

/* Compare two StringSlices */
int string_slice_compare(StringSlice s0, StringSlice s1);
bool string_slice_equals(StringSlice s0, StringSlice s1);

