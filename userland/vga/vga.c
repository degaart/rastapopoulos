#include <runtime.h>
#include <port.h>
#include <debug.h>
#include <string.h>
#include <malloc.h>
#include <io.h>

const char* const strings[] = {
    "This was a triumph\n",
    "I'm making a note here: \"HUGE SUCCESS\"\n",
    "It's hard to overstate my satisfaction\n",
    "Aperture Science\n",
    "We do what we must because we can\n",
    "For the good of all of us, except the ones who are dead\n",
    "\n",
    "But there's no sense crying over every mistake\n",
    "You just keep on trying 'til you run out of cake\n",
    "And the Science gets done\n",
    "And you make a neat gun\n",
    "For the people who are still alive\n",
    "\n",
    "I'm not even angry\n",
    "I'm being so sincere right now\n",
    "Even though you broke my heart\n",
    "And killed me and tore me to pieces\n",
    "And threw every piece into a fire\n",
    "As they burned it hurt because I was so happy for you\n",
    "\n",
    "Now these points of data make a beautiful line\n",
    "And we're out of beta, we're releasing on time\n",
    "So I'm GLaD I got burned\n",
    "Think of all the things we learned\n",
    "For the people who are still alive\n",
    "\n",
    "Go ahead and leave me\n",
    "I think I prefer to stay inside\n",
    "Maybe you'll find someone else to help you\n",
    "Maybe Black Mesa\n",
    "That was a joke, haha, fat chance\n",
    "Anyway, this cake is great, it's so delicious and moist\n",
    "\n",
    "Look at me still talking when there's Science to do. When I look out there, it makes me GLaD I'm not you\n",
    "I've experiments to run\n",
    "There is research to be done\n",
    "On the people who are still alive\n",
    "\n",
    "And believe me I am still alive\n",
    "I'm doing science and I'm still alive\n",
    "I feel fantastic and I'm still alive\n",
    "While you're dying I'll be still alive\n",
    "And when you're dead I will be still alive\n",
    "Still alive, still alive\n"
};

#define LOWMEM_START            0x1000
#define VGA_BASE                0xB8000

#define COLOR_BLACK             0
#define COLOR_BLUE              1
#define COLOR_GREEN             2
#define COLOR_CYAN              3
#define COLOR_RED               4
#define COLOR_MAGENTA           5
#define COLOR_BROWN             6
#define COLOR_LIGHT_GRAY        7
#define COLOR_DARK_GRAY         8
#define COLOR_LIGHT_BLUE        9
#define COLOR_LIGHT_GREEN       10
#define COLOR_LIGHT_CYAN        11
#define COLOR_LIGHT_RED         12
#define COLOR_LIGHT_MAGENTA     13
#define COLOR_YELLOW            14
#define COLOR_WHITE             15

#define clamp(x, min, max)  \
    do {                    \
        if((x) < (min))         \
            (x) = (min);        \
        if((x) > (max))         \
            (x) = (max);        \
    } while(0)

struct coords {
    int row, col;
};
static struct coords cursor;

static uint16_t* backbuffer;

int vga_current_mode()
{
    struct int10_regs regs = {
        .eax = 0x0F00
    };
    int10(&regs);
    
    int mode = (regs.eax & 0xFF) & ~(1 << 7);
    return mode;
}

int vga_set_mode(unsigned mode)
{
    struct int10_regs regs = {
        .eax = 0x00 | (mode & 0xFF)
    };
}

struct coords vga_cursor_pos()
{
    struct int10_regs regs = {
        .eax = 0x0300,
        .ebx = 0
    };
    int10(&regs);

    struct coords result;
    result.row = (regs.edx >> 8) & 0xFF;
    result.col = regs.edx & 0xFF;
    return result;
}

void vga_set_cursor_pos(int row, int col)
{
    struct int10_regs regs = {
        .eax = 0x0200,
        .ebx = 0
    };
    regs.edx = ((row & 0xFF) << 8) | (col & 0xFF);
    int10(&regs);

    trace("set_cursor_pos(%d, %d)", col, row);
}

static void vga_flip()
{
    memcpy((void*)VGA_BASE, backbuffer, sizeof(uint16_t) * 80 * 25);

    vga_set_cursor_pos(cursor.row, cursor.col);
}

void vga_write_char(int col, int row, int fore, int back, int c)
{
    clamp(col, 0, 79);
    clamp(row, 0, 24);

    unsigned attrib = ((back & 0xF) << 4) | (fore & 0x0F);
    backbuffer[row * 80 + col] = (c & 0xFF) | (attrib << 8);
}

void vga_scroll(int rows)
{
    rows %= 25;

    memcpy(backbuffer, backbuffer + (rows * 80), (80 * 25 * 2) - (rows * 80 * 2));
    for(int i = 0; i < 80; i++) {
        vga_write_char(i, 24, COLOR_LIGHT_GRAY, COLOR_BLACK,' ');
    }

    if(cursor.row >= rows)
        cursor.row -= rows;
    else
        cursor.row = 0;
}

void vga_type(int fore, int back, int c)
{
    if(c != '\n') {
        /* Write at current cursor pos */
        vga_write_char(cursor.col, cursor.row, fore, back, c);

        /* Advance cursor */
        cursor.col++;

        /* If cursor > 80, move it lower */
        if(cursor.col >= 80) {
            cursor.row++;
            cursor.col = 0;
        }
        
        /* If cursor goes beyond screen, scroll screen */
        if(cursor.row >= 25) {
            vga_scroll(1);
        }
    } else {
        cursor.row++;
        cursor.col = 0;
        if(cursor.row >= 25) {
            vga_scroll(1);
        }
    }

    for(int i = 0; i < 65536 * 2; i++) {
        io_delay();
    }
}

void vga_write_string(const char* str, int fore, int back)
{
    while(*str) {
        vga_type(fore, back, *str);
        vga_flip();
        str++;
    }
}

void main()
{
    trace("VGA driver started");

    /* Map lowmem */
    int ret = mmap_phys(LOWMEM_START, 
                        (void*)LOWMEM_START, 
                        0x100000 - LOWMEM_START, 
                        PROT_READ|PROT_WRITE);
    if(ret) {
        panic("mmap_phys failed: %d", ret);
    }

    /* Get current video mode, and set to mode 3 if not mode 3 */
    int mode = vga_current_mode();
    if(mode != 0x03) {
        trace("Setting vga mode");
        int ret = vga_set_mode(0x03);
        if(ret) {
            panic("vga_set_mode failed");
        }
    }

    /* init cursor pos */
    cursor = vga_cursor_pos();
    trace("Cursor pos: (%d, %d)", cursor.col, cursor.row);

    /* Init backbuffer */
    size_t backbuffer_size = sizeof(uint16_t) * 80 * 25;
    backbuffer = malloc(backbuffer_size);
    memcpy(backbuffer, (const void*)VGA_BASE, 80 * 25 * 2);

    /* Write sum good stuff */
    int line = 0;
    size_t nlines = sizeof(strings) / sizeof(strings[0]);
    for(line = 0; line < nlines; line++) {
        vga_write_string(strings[line], COLOR_LIGHT_GRAY, COLOR_BLACK);
    }
    while(1);
}


