#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <stdarg.h>


#define countof(a) sizeof(a) / sizeof(a[0])
    
void reboot();
extern void halt();

