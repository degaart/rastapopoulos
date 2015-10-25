#include <string.h>

/* Stolen from FreeBSD 10 */
size_t strlcpy(char * restrict dst, const char * restrict src, size_t size) {
    char *d = dst;
    const char *s = src;
    unsigned n = size;

    /* Copy as many bytes as will fit */
    if (n != 0) {
        while (--n != 0) {
            if ((*d++ = *s++) == '\0')
                break;
        }
    }

    /* Not enough room in dst, add NUL and traverse rest of src */
    if (n == 0) {
        if (size != 0)
            *d = '\0';      /* NUL-terminate dst */
        while (*s++)
            ;
    }

    return(s - src - 1);    /* count does not include NUL */
}

