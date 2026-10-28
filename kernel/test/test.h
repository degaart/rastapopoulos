#pragma once

#include "../kernel.h"
#include "../trace.h"

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            trace("CHECK failed: %s", #cond);                                 \
            debugexit(2);                                                     \
        }                                                                     \
    } while (0)

