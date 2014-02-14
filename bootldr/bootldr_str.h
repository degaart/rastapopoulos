#ifndef _BOOTLDR_STR_H_
#define _BOOTLDR_STR_H_

void write_char(int ch, int page, int color);
void write_string(const char* str);
void write_uint16(uint16_t i);
int memcmp(const void* s0, const void* s1, uint16_t siz);

#endif //_BOOTLDR_STR_H_

