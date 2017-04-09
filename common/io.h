#pragma once

#include <stdint.h>

extern void outb(uint32_t port, uint32_t val);
extern uint32_t inb(uint32_t port);
extern void outw(uint32_t port, uint32_t val);
extern uint32_t inw(uint32_t port);

