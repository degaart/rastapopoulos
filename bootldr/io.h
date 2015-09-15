#ifndef _IO_H_
#define _IO_H_

unsigned inb(unsigned port);
void outb(unsigned port, unsigned byte);
void serial_write_char(int ch);
void serial_write_string(const char* str); 

#endif //_IO_H_


