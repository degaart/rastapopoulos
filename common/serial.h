#pragma once

#define SERIAL_COM1 0x3F8

void serial_write_char(char ch);
void serial_write_string(const char* s);
