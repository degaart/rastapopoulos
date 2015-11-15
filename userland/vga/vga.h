/*
    Terminal output functions
*/
#ifndef _VGA_H_
#define _VGA_H_

    #include <stdarg.h>

    #define VGA_BASE 0xB8000
    
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
    
    #define PANIC_COLOR (COLOR_LIGHT_RED|0x10)
    
    void get_cursor_pos(unsigned* x, unsigned *y);
    void set_cursor_pos(unsigned x, unsigned y);
    void set_char_attr_at(unsigned x, unsigned y, int ch, enum VGA_COLOR attr);
    void scroll_screen();
    void write_string_attr(const char* str, unsigned size, enum VGA_COLOR attr);
    void clear_screen();
    void vga_init();
    
#endif //_VGA_H_


