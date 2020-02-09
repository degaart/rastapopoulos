#pragma once

#include <stdint.h>

#define KERNEL_START ((unsigned char*)&__KERNEL_START__)
#define KERNEL_END ((unsigned char*)&__KERNEL_END__)
#define TEXT_START ((unsigned char*)&__TEXT_START__)
#define TEXT_END ((unsigned char*)&__TEXT_END__)
#define RODATA_START ((unsigned char*)&__RODATA_START__)
#define RODATA_END ((unsigned char*)&__RODATA_END__)
#define DATA_START ((unsigned char*)&__DATA_START__)
#define DATA_END ((unsigned char*)&__DATA_END__)
#define BSS_START ((unsigned char*)&__BSS_START__)
#define BSS_END ((unsigned char*)&__BSS_END__)
#define USER_START ((unsigned char*)&__USER_START__)
#define USER_END ((unsigned char*)&__USER_END__)

extern unsigned char __KERNEL_START__;
extern unsigned char __KERNEL_END__;
extern unsigned char __TEXT_START__;
extern unsigned char __TEXT_END__;
extern unsigned char __RODATA_START__;
extern unsigned char __RODATA_END__;
extern unsigned char __DATA_START__;
extern unsigned char __DATA_END__;
extern unsigned char __BSS_START__;
extern unsigned char __BSS_END__;
extern unsigned char __USER_START__;
extern unsigned char __USER_END__;

#define KERNEL_BASE 0xC0000000

