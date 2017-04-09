#pragma once

#include <stdint.h>

struct spinlock {
    volatile uint32_t l;
};
typedef struct spinlock spinlock_t;
#define SPINLOCK_INIT (struct spinlock) { .l = 0 }

