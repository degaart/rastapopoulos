#pragma once

#include "rastaldr.h"

#define assert(cond)                                                          \
    if (!(cond)) {                                                            \
        panic("Assert failed at %s:%u\n%s", __FILE__, __LINE__, #cond);       \
    }

