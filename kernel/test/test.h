#pragma once

#include "../kernel.h"
#include "../trace.h"

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            trace("CHECK failed: %s", #cond);                                 \
            printf("CHECK failed: %s\n", #cond);                              \
            debugexit(2);                                                     \
            halt();                                                           \
        }                                                                     \
    } while (0)

