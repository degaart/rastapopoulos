#include "kmalloc.h"
#include "string.h"

char* strdup(const char* str1)
{
    size_t size = strlen(str1);
    char* result = kmalloc(size + 1);
    if (!result)
        return NULL;
    memcpy(result, str1, size + 1);
    return result;
}

