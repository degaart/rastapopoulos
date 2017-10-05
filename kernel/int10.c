#include "kdebug.h"
#include "string.h"
#include "int10_stub.h"
#include "io.h"
#include "sin_acos.h"
#include "random.h"
#include "util.h"

#define VGA_80x25           0x03
#define VGA_320x200x8       0x13

#define VGA_BLACK               0
#define VGA_BLUE                1
#define VGA_GREEN               2
#define VGA_CYAN                3
#define VGA_RED                 4
#define VGA_MAGENTA             5
#define VGA_BROWN               6
#define VGA_LIGHT_GRAY          7
#define VGA_DARK_GRAY           8
#define VGA_LIGHT_BLUE          9
#define VGA_LIGHT_GREEN         10  
#define VGA_LIGHT_CYAN          11
#define VGA_LIGHT_RED           12
#define VGA_LIGHT_MAGENTA       13
#define VGA_YELLOW              14
#define VGA_WHITE               15

#define VGA_BASE 0xA0000

#define abs(x) ((x) > 0 ? (x) : -(x))
#define sgn(x) ((x<0)?-1:((x>0)?1:0))
#define swap(x, y) do { int tmp = y; y = x; x = tmp; } while(0)
#define clamp(x, min, max)  \
    do {                    \
        if(x < min)         \
            x = min;        \
        if(x > max)         \
            x = max;        \
    } while(0)

static uint32_t rng_state;


struct int10_regs {
    uint32_t eax, ebx, ecx, edx;
    uint32_t ebp, esi, edi;
    uint32_t es, fs, gs;
    uint32_t reserved;
} __attribute((packed));

void int10(struct int10_regs*);

struct coord {
    int row, col;
};

static struct coord get_cursor_pos()
{
    struct int10_regs regs = { .eax = 0x0300 };
    int10(&regs);

    struct coord result;
    result.row = (regs.edx & 0xFF00) >> 8;
    result.col = regs.edx & 0xFF;
    return result;
}

static void set_cursor_pos(int x, int y)
{
    /*
     * AH = 02h
     * BH = page number
     * 0-3 in modes 2&3
     * 0-7 in modes 0&1
     * 0 in graphics modes
     * DH = row (00h is top)
     * DL = column (00h is left)
     */
    struct int10_regs regs = {
        .eax = 0x0200,
        .ebx = 0,
        .edx = ((y & 0xFF) << 8) | (y & 0xFF)
    };
    int10(&regs);
}

static void write_char(int ch, int row, int col)
{
    /*
        AH = 0Ah
        AL = character to display
        BH = page number (00h to number of pages - 1) (see #00010)
        background color in 256-color graphics modes (ET4000)
        BL = attribute (PCjr, Tandy 1000 only) or color (graphics mode)
        if bit 7 set in <256-color graphics mode, character is XOR'ed
        onto screen
        CX = number of times to write character

        Return:
        Nothing
     */
    struct int10_regs regs;
    regs.eax = 0x0E | (ch & 0xFF);
    regs.ebx = 0x0007;
    int10(&regs);
}

static void write_string(const char* str, int attribute, int row, int col)
{
    /*
        VIDEO - WRITE STRING (AT and later,EGA)

        AH = 13h
        AL = write mode

        bit 0:
        Update cursor after writing

        bit 1:
        String contains alternating characters and attributes

        bits 2-7:
        Reserved (0).
        BH = page number.
        BL = attribute if string contains only characters.
        CX = number of characters in string.
        DH,DL = row,column at which to start writing.
        ES:BP -> string to write
     */
    size_t len = strlen(str) & 0xFF; /* 256 chars limit */
    struct int10_regs regs = {
        .eax = 0x1301,
        .ebx = attribute & 0xFF,
        .ecx = len,
        .edx = ((row & 0xFF) << 8) | (col & 0xFF),
        .ebp = 0x7b00,
        .es = 0,
    };
    memcpy((void*)0x7b00, str, len);
    int10(&regs);
}


static void set_video_mode(int mode)
{
    struct int10_regs regs = {
        .eax = (mode & 0xFF)
    };
    int10(&regs);
}

static void clearscreen(int color)
{
    memset((void*)VGA_BASE, (color & 0xFF), 320 * 200);
}

static void putpixel(int x, int y, int col)
{
    clamp(x, 0, 319);
    clamp(y, 0, 199);

    unsigned char* ptr = (unsigned char*)VGA_BASE;
    int offset = (320 * y) + x;
    ptr[offset] = col & 0xFF;

#if 0
    for(int i = 0; i < 65536; i++) {
        io_delay();
    }
#endif
}

static void drawhline(int x1, int x2, int y, int color)
{
    if(x1 > x2)
        swap(x1, x2);

    clamp(x1, 0, 319);
    clamp(x2, 0, 319);
    clamp(y, 0, 199);

    unsigned char* ptr = (unsigned char*)(VGA_BASE + (y * 320) + x1);
    memset(ptr, (color & 0xFF), x2 - x1);
}

static void drawvline(int x, int y1, int y2, int color)
{
    if(y1 > y2) {
        swap(y1, y2);
    }

    clamp(x, 0, 319);
    clamp(y1, 0, 199);
    clamp(y2, 0, 199);

    color &= 0xFF;
    unsigned char* ptr = (unsigned char*)(VGA_BASE + (y1 * 320) + x);
    for(int i = y1; i <= y2; i++) {
        *ptr = color;
        ptr += 320;
    }
}


static void drawline(int x1, int y1, int x2, int y2, int color)
{
    if(x1 == x2) {
        drawvline(x1, y1, y2, color);
        return;
    } else if(y1 == y2) {
        drawhline(x1, x2, y2, color);
        return;
    }

    int i,dx,dy,sdx,sdy,dxabs,dyabs,x,y,px,py;

    dx=x2-x1;      /* the horizontal distance of the line */
    dy=y2-y1;      /* the vertical distance of the line */
    dxabs=abs(dx);
    dyabs=abs(dy);
    sdx=sgn(dx);
    sdy=sgn(dy);
    x=dyabs>>1;
    y=dxabs>>1;
    px=x1;
    py=y1;

    putpixel(px, py, color);

    if (dxabs>=dyabs) /* the line is more horizontal than vertical */
    {
        for(i=0;i<dxabs;i++)
        {
            y+=dyabs;
            if (y>=dxabs)
            {
                y-=dxabs;
                py+=sdy;
            }
            px+=sdx;
            putpixel(px,py,color);
        }
    }
    else /* the line is more vertical than horizontal */
    {
        for(i=0;i<dyabs;i++)
        {
            x+=dxabs;
            if (x>=dyabs)
            {
                x-=dyabs;
                px+=sdx;
            }
            py+=sdy;
            putpixel(px,py,color);
        }
    }
}

static void drawpolygon(int nvert, const int* vert, int color)
{
    for(int i = 0; i < nvert - 1; i++) {
        drawline(vert[i * 2], vert[(i * 2) + 1],
                 vert[(i + 1) * 2], vert[((i + 1) * 2) + 1],
                 color);
    }
    drawline(vert[0], vert[1],
             vert[(nvert - 1) * 2], vert[((nvert - 1) * 2) + 1], 
             color);
}

static void drawrect(int left, int top, int right, int bottom, int color)
{
    drawline(left, top, right, top, color);
    drawline(right, top, right, bottom, color);
    drawline(right, bottom, left, bottom, color);
    drawline(left, bottom, left, top, color);
}


static void fillrect(int left, int top, int right, int bottom, int color)
{
    if(top > bottom)
        swap(top, bottom);
    if(left > right)
        swap(left, right);

    clamp(left, 0, 319);
    clamp(top, 0, 199);
    clamp(right, 0, 319);
    clamp(bottom, 0, 199);

    int top_offset = (top * 320) + left;
    int bottom_offset = (bottom * 320) + left;
    int width = right - left + 1;

    unsigned char* ptr = (unsigned char*)VGA_BASE;
    for(int i = top_offset; i <= bottom_offset; i += 320) {
        memset(ptr + i, color, width);
    }
}

static void drawcircle(int x, int y, int radius, int color)
{
    if(radius == 0)
        return;

    int32_t n = 0, invradius = (1 * 0x10000) / radius;
    int dx = 0, dy = radius - 1;
    int dxoffset, dyoffset, offset = (y * 320) + x;

    unsigned char* ptr = (unsigned char*)VGA_BASE;
    while(dx <= dy) {
        dxoffset = dx * 320;
        dyoffset = dy * 320;
        ptr[offset+dy-dxoffset] = color;  /* octant 0 */
        ptr[offset+dx-dyoffset] = color;  /* octant 1 */
        ptr[offset-dx-dyoffset] = color;  /* octant 2 */
        ptr[offset-dy-dxoffset] = color;  /* octant 3 */
        ptr[offset-dy+dxoffset] = color;  /* octant 4 */
        ptr[offset-dx+dyoffset] = color;  /* octant 5 */
        ptr[offset+dx+dyoffset] = color;  /* octant 6 */
        ptr[offset+dy+dxoffset] = color;  /* octant 7 */
        dx++;
        n+=invradius;
        dy = (int)((radius * sin_acos[(int)(n>>6)]) >> 16);
    }
}

static void fillcircle(int x, int y, int radius, int color)
{
    if(radius == 0)
        return;

    int32_t n = 0, invradius = (1 * 0x10000) / radius;
    int dx = 0, dy = radius - 1;
    int dxoffset, dyoffset, offset = (y * 320) + x;

    unsigned char* ptr = (unsigned char*)VGA_BASE;
    while(dx <= dy) {
        dxoffset = dx * 320;
        dyoffset = dy * 320;
        for(int i = dy; i >= dx; i--, dyoffset -= 320) {
            ptr[offset+i -dxoffset] = color;  /* octant 0 */
            ptr[offset+dx-dyoffset] = color;  /* octant 1 */
            ptr[offset-dx-dyoffset] = color;  /* octant 2 */
            ptr[offset-i -dxoffset] = color;  /* octant 3 */
            ptr[offset-i +dxoffset] = color;  /* octant 4 */
            ptr[offset-dx+dyoffset] = color;  /* octant 5 */
            ptr[offset+dx+dyoffset] = color;  /* octant 6 */
            ptr[offset+i +dxoffset] = color;  /* octant 7 */
        }
        dx++;
        n+=invradius;
        dy = (int)((radius * sin_acos[(int)(n>>6)]) >> 16);
    }
}

static int random(int lo, int max)
{
    int base = (int)(xorshift32(&rng_state) & 0xFFFF);

    int delta = max - lo;
    int result = lo + ((delta * base) / 0xFFFF);
    assert(result >= lo);
    assert(result <= max);
    return result;
}

void test_int10()
{
    trace("Testing int10 calls");

    /* Initialize RNG */
    rng_state = rdtsc() & 0xFFFFFFFF;

    /* Copy stub to 0x7C00 */
    memcpy((void*)0x7C00, obj_int10_stub_bin, sizeof(obj_int10_stub_bin));

    /* Set video mode (320x200x8) */
    set_video_mode(VGA_320x200x8);
    
    /* Clear screen */
    clearscreen(VGA_WHITE);

    /* Draw random shapes */
    while(1) {
        int shape = random(0, 6);
        switch(shape) {
            case 0:
                {
                    for(int i = 0; i < 30; i++) {
                        putpixel(random(0, 319), random(0, 199), random(0, 255));
                    }
                }
                break;
            case 1:
                drawline(random(0, 319), random(0, 199), 
                         random(0, 319), random(0, 199), 
                         random(0, 255));
                break;
            case 2:
                {
                    int vertices[3 * 2];
                    for(int i = 0; i < 3; i++) {
                        vertices[i * 2] = random(0, 319);
                        vertices[(i * 2) + 1] = random(0, 199);
                    }
                    drawpolygon(3, vertices, random(0, 255));
                }
                break;
            case 3:
            case 4:
                {
                    int x1 = random(0, 319);
                    int y1 = random(0, 199);
                    int x2 = random(0, 319);
                    int y2 = random(0, 199);
                    int col1 = random(0, 255);
                    int col2 = random(0, 255);

                    if(shape == 3) {
                        drawrect(x1, y1, x2, y2, col1);
                    } else {
                        fillrect(x1 + 1, y1 + 1, x2 - 1, y2 - 1, col2);
                    }
                }
                break;
            case 5:
            case 6:
                {
                    int x = random(0, 319);
                    int y = random(0, 199);
                    int radius = random(0, 99);
                    int col = random(0, 255);
                    if(shape == 5) {
                        drawcircle(x, y, radius, col);
                    } else {
                        fillcircle(x, y, radius, col);
                    }
                }
                break;
        }
    }
}


