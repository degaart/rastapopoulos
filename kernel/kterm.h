/*
	Terminal output functions
*/
#ifndef _KTERM_H_
#define _KTERM_H_

	enum VGA_COLOR {
		COLOR_BLACK = 0,
		COLOR_BLUE = 1,
		COLOR_GREEN = 2,
		COLOR_CYAN = 3,
		COLOR_RED = 4,
		COLOR_MAGENTA = 5,
		COLOR_BROWN = 6,
		COLOR_LIGHT_GREY = 7,
		COLOR_DARK_GREY = 8,
		COLOR_LIGHT_BLUE = 9,
		COLOR_LIGHT_GREEN = 10,
		COLOR_LIGHT_CYAN = 11,
		COLOR_LIGHT_RED = 12,
		COLOR_LIGHT_MAGENTA = 13,
		COLOR_LIGHT_BROWN = 14,
		COLOR_WHITE = 15,
	};
	
	void get_cursor_pos(unsigned* x, unsigned *y);
	void set_cursor_pos(unsigned x, unsigned y);
	void set_char_attr_at(unsigned x, unsigned y, int ch, enum VGA_COLOR attr);
	void scroll_screen();
	void write_string_attr(const char* str, enum VGA_COLOR attr);
	void write_value_attr(uint32_t value, enum VGA_COLOR attr);
	void write_string(const char* str);
	void clear_screen();
	void term_init();
	
	#define DUMP32(v) \
		write_string_attr(#v ": ", COLOR_LIGHT_GREY); \
		write_value_attr(v, COLOR_LIGHT_GREY); \
		write_string_attr("\n", COLOR_LIGHT_GREY)
	
#endif //_KTERM_H_


