#include "string_slice.h"

int string_slice_compare(StringSlice s0, StringSlice s1)
{
    size_t len = s0.len < s1.len ? s0.len : s1.len;

    for (size_t i = 0; i < len; ++i) {
        unsigned char c0 = (unsigned char)s0.buf[i];
        unsigned char c1 = (unsigned char)s1.buf[i];

        if (c0 != c1)
            return c0 < c1 ? -1 : 1;
    }

    return (s0.len > s1.len) - (s0.len < s1.len);
}

bool string_slice_equals(StringSlice s0, StringSlice s1)
{
    return string_slice_compare(s0, s1) == 0;
}

