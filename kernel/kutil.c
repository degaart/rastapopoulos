#include <stdint.h>
#include "kterm.h"
#include "kutil.h"

void panic(const char* file, int line, const char* function, const char* message, ...) {
	write_format_attr(PANIC_COLOR, "Kernel panic at %s[%d](%s): ", file, line, function);
	
	va_list args;
	va_start(args, message);
	write_format_attr_v(PANIC_COLOR, message, args);
	va_end(args);
	
	write_format_attr(PANIC_COLOR, "\n");	/* just for the sake of it */
	_halt();
}

void trace(const char* file, int line, const char* function, const char* message, ...) {
	write_format("(%s:%u): ", file, line);
	
	va_list args;
	va_start(args, message);
	write_format_attr_v(COLOR_LIGHT_GREY,message, args);
	va_end(args);
	
	write_format("\n");	/* just for the sake of it */
}

