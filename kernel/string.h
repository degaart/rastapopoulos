#ifndef _STRING_H_
#define _STRING_H_

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

class String {
public:
	static void itoa(char* str, unsigned n);
	
	static void itox(char* str, unsigned n);

	static uint32_t xtoa(const char* str);
	
	typedef void (*format_callback)(int, void*);
	static int format(
	    format_callback callback, 
	    void* callback_params,
	    const char* fmt,
	    ...
	) __attribute__ ((format (printf, 3, 4)));

	static int formatv(
	    format_callback callback, 
	    void* callback_params,
	    const char* fmt,
	    va_list args
	);
};

extern "C" {
	void memset(void* buffer, int ch, uint32_t size);
	void bzero(void* buffer, uint32_t size);
	void memcpy(void* dest, const void* src, size_t size);
	int memcmp(const void* p0, const void* p1, size_t size);

	unsigned strlcpy(char* dst, const char* src, unsigned size);
	unsigned strlcat(char* dst, const char* src, unsigned size);
	char* strdup(const char* str);
	size_t strlen(const char* str);
	int strcmp(const char* s0, const char* s1);
	int vsnprintf(char* buffer, size_t size, const char* fmt, va_list args);
	int snprintf(char* buffer, size_t size, const char* fmt, ...) __attribute__ ((format (printf, 3, 4)));;
	int vsncatf(char* buffer, size_t size, const char* fmt, va_list args);
	int sncatf(char* buffer, size_t size, const char* fmt, ...) __attribute__ ((format (printf, 3, 4)));;
}

#endif // _STRING_H_
