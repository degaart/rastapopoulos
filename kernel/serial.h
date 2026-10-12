#pragma once

void serial_init(void);
void serial_putchar(char c);
void serial_write(const char* text);
void serial_write_string(const char* text, int len);

