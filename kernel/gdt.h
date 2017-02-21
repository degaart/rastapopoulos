#pragma once

#define KERNEL_CODE_SEG     0x08
#define KERNEL_DATA_SEG     0x10
#define USER_CODE_SEG       0x18
#define USER_DATA_SEG       0x20
#define TSS_SEG             0x28

#define RPL0                0x0
#define RPL1                0x1
#define RPL2                0x2
#define RPL3                0x3

void gdt_init();
void gdt_flush(void* gdtr);
void tss_flush();

