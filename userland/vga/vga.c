/*
    VGA Terminal output functions
*/
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <rasta.h>
#include "vga.h"

/* Utility macros *********************************************************/
#define VGA_POS(x,y) (vga_mem+(((y*80)+x)*2))
#define VGA_ATTR(x,y) (vga_mem+(((y*80)+x)*2)+1)
#define VGA_WRITE_CHAR(x,y,ch) (VGA_POS(x,y)[0] = ch)
#define VGA_WRITE_ATTR(x,y,attr) (VGA_ATTR(x,y)[0] = attr)

/* Constants **************************************************************/ 
static unsigned char* vga_mem = (unsigned char*)VGA_BASE;

/* Variables **************************************************************/ 
static unsigned cursor_x, cursor_y;

/* Get current VGA cursor position */
void get_cursor_pos(unsigned* x, unsigned *y) {
    /* LSB of cursor pos */
    rs_outb(0x3D4, 0xF);
    uint8_t pos_lsb = rs_inb(0x3D5);
    
    /* MSB of cursor pos */
    rs_outb(0x3D4, 0xE);
    uint8_t pos_msb = rs_inb(0x3D5);
    
    /* Cursor address: (y*80)+x */
    uint16_t pos = (pos_lsb|(pos_msb << 8));
    *x = pos % 80;
    *y = pos / 80;
}

/* Set current VGA cursor position */
void set_cursor_pos(unsigned x, unsigned y) {
    /* Pos: (y*80)+x */
    uint16_t pos = (y*80)+x;
    
    /* Set LSB */
    rs_outb(0x3D4, 0xF);
    rs_outb(0x3D5, pos & 0x00FF);
    
    /* Set MSB */
    rs_outb(0x3D4, 0xE);
    rs_outb(0x3D5, (pos & 0xFF00)>>8);
}

/* Emit character and attribute at specific position in screen */
void set_char_attr_at(unsigned x, unsigned y, int ch, enum VGA_COLOR attr) {
    VGA_WRITE_CHAR(x,y,ch);
    VGA_WRITE_ATTR(x,y,attr);
}

/*
    Scroll screen up by 1 line
    and update cursor position
*/
void scroll_screen() {
    memcpy(VGA_POS(0,0), VGA_POS(0, 1), 80*2*24);
    
    /* Clear last line */
    for(int i=0; i<80; i++) {
        *VGA_POS(i,24) = ' ';
        *VGA_ATTR(i,24) = COLOR_LIGHT_GREY;
    }

    /* Set cursor pos */
    cursor_y--;
    set_cursor_pos(cursor_x, cursor_y);
}

/*
    Write string at current cursor position, and update cursor position
*/
void write_string_attr(const char* str, unsigned size, enum VGA_COLOR attr) {
    while(size--) {
        if(*str == '\n') {
            cursor_x = 0;
            cursor_y++;
            if(cursor_y>24) {
                scroll_screen();
            }
        } else {
            VGA_WRITE_CHAR(cursor_x,cursor_y, *str);
            VGA_WRITE_ATTR(cursor_x,cursor_y, attr);
            
            cursor_x++;
            if(cursor_x == 80) {
                cursor_y++;
                if(cursor_y>24) {
                    scroll_screen();
                }
                cursor_x = 0;
            }
        }
        str++;
    }
    set_cursor_pos(cursor_x, cursor_y);
}

/* Clear screen */
void clear_screen() {
    for(unsigned y=0; y<25; y++) {
        for(unsigned x=0; x<80; x++) {
            *VGA_POS(x,y)=' ';
            *VGA_ATTR(x,y)=COLOR_LIGHT_GREY;
        }
    }
    cursor_y = cursor_x = 0;
    set_cursor_pos(0,0);
}

void vga_init() {
    get_cursor_pos(&cursor_x, &cursor_y);
}

