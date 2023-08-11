#pragma once

#include <stdint.h>

#define GDT_CS_KERNEL 0x08
#define GDT_DS_KERNEL 0x10
#define GDT_CS_USER   0x18
#define GDT_DS_USER   0x20
#define GDT_TSS       0x28

void tss_set_esp0(const void* esp0);
void* tss_get_esp0(void);
void iomap_allow_all();
void iomap_deny_all();
void iomap_deny(uint16_t port);
void iomap_allow(uint16_t port);
void gdt_init(void);
