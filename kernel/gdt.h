#pragma once

#include <stdint.h>

#define KERNEL_CODE_SEG     0x08
#define KERNEL_DATA_SEG     0x10
#define USER_CODE_SEG       0x18
#define USER_DATA_SEG       0x20
#define TSS_SEG             0x28

#define RPL0                0x0
#define RPL1                0x1
#define RPL2                0x2
#define RPL3                0x3
#define IOMAP_SIZE          ((65536 / 8) + 1)

void gdt_init();
void tss_set_esp0(void* esp0);
extern void tss_flush();
extern void switch_to_usermode(void* user_stack, void* entry);


