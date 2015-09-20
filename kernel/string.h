#ifndef _STRING_H_
#define _STRING_H_

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

class String {
public:
	static void itoa(char* str, unsigned n);
	
	static void itox(char* str, unsigned n);
	
	typedef void (*format_callback)(int, void*);
	static void format(
	    format_callback callback, 
	    void* callback_params,
	    const char* fmt,
	    ...
	);
	static void formatv(
	    format_callback callback, 
	    void* callback_params,
	    const char* fmt,
	    va_list args
	);
};

void memset(void* buffer, int ch, uint32_t size);
void bzero(void* buffer, uint32_t size);
void memcpy(void* dest, const void* src, size_t size);
void strcpy(char* dest, const char* src);

#endif // _STRING_H_
