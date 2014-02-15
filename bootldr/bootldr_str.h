#ifndef _BOOTLDR_STR_H_
#define _BOOTLDR_STR_H_

void write_char(int ch, int page, int color);
void write_string(const char* str);
void write_uint16(uint16_t i);
int memcmp(const void* s0, const void* s1, uint16_t siz);

#define DUMP16(s) \
	write_string(#s ": "); \
	write_uint16(s); \
	write_string("\r\n")

#endif //_BOOTLDR_STR_H_

