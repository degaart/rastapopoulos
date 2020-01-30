#pragma once

#include <stdint.h>

#define KERNEL_START ((void*)&__KERNEL_START__)
#define KERNEL_END ((void*)&__KERNEL_END__)

extern unsigned char __KERNEL_START__;
extern unsigned char __KERNEL_END__;

