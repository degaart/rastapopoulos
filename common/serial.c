#include "serial.h"
#include "io.h"

void serial_write_char(char ch) { outb(SERIAL_COM1, ch); }

void serial_write_string(const char *s) {
  while (*s) {
    serial_write_char(*s);
    s++;
  }
}
