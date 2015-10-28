#ifndef _RASTA_H_
#define _RASTA_H_

#include <stdint.h>
#include "../../kernel/message.h"

#define RS_PORT_VGA                0x1

#define RS_MSG_VGA_WRITE_STRING    0x1

#define MMAP_WRITABLE           0x2

void rs_yield();
void rs_trace(const char* str, ...);
void rs_mmap(const void* addr, uint32_t physical, uint32_t flags);
void rs_outb(unsigned port, unsigned ch);
unsigned rs_inb(unsigned port);
void rs_outw(unsigned port, unsigned val);
unsigned rs_inw(unsigned port);

uint32_t rs_port_open(uint32_t port_number);
uint32_t rs_port_close(uint32_t port_number);
uint32_t rs_port_send(uint32_t port_number, const struct Message_t* msg);
uint32_t rs_port_read(uint32_t port_number, struct Message_t* buffer);

#endif

