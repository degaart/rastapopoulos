#ifndef _STDIO_H_
#define _STDIO_H_

#include <stdarg.h>

int vprintf(const char * restrict format, va_list ap);
int printf(const char * restrict format, ...);

#endif //_STDIO_H_

