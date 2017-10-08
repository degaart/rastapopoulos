#include "kdebug.h"
#include "string.h"
#include "int10_stub.h"
#include "io.h"
#include "sin_acos.h"
#include "random.h"
#include "util.h"
#include "vmm.h"
#include "kmalloc.h"

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
        if((x) < (min))         \
            (x) = (min);        \
        if((x) > (max))         \
            (x) = (max);        \
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

static void delay()
{
    for(int i = 0; i < 0xFFFF; i++) {
        io_delay();
    }
}


/*********************************************************************
 * VBE funcs
 *********************************************************************/

/* Start of conventional memory (29Kb) */
#define LOWMEM_START    0x500
#define SVGA_FB         0x400000

struct vbe_info {
    char signature[4];
    uint16_t version;
    uint32_t oem_name;
    uint32_t capabilities;
    uint16_t modes[2];
    uint16_t memsize64k;
    uint16_t rev;
    uint32_t vendor;
    uint32_t product;
    uint32_t product_rev;
} __attribute__((packed));

struct vbe_modeinfo {
	uint16_t attributes;		// deprecated, only bit 7 should be of interest to you, and it indicates the mode supports a linear frame buffer.
	uint8_t window_a;			// deprecated
	uint8_t window_b;			// deprecated
	uint16_t granularity;		// deprecated; used while calculating bank numbers
	uint16_t window_size;
	uint16_t segment_a;
	uint16_t segment_b;
	uint32_t win_func_ptr;		// deprecated; used to switch banks from protected mode without returning to real mode
	uint16_t pitch;			// number of bytes per horizontal line
	uint16_t width;			// width in pixels
	uint16_t height;			// height in pixels
	uint8_t w_char;			// unused...
	uint8_t y_char;			// ...
	uint8_t planes;
	uint8_t bpp;			// bits per pixel in this mode
	uint8_t banks;			// deprecated; total number of banks in this mode
	uint8_t memory_model;
	uint8_t bank_size;		// deprecated; size of a bank, almost always 64 KB but may be 16 KB...
	uint8_t image_pages;
	uint8_t reserved0;
 
	uint8_t red_mask;
	uint8_t red_position;
	uint8_t green_mask;
	uint8_t green_position;
	uint8_t blue_mask;
	uint8_t blue_position;
	uint8_t reserved_mask;
	uint8_t reserved_position;
	uint8_t direct_color_attributes;
 
	uint32_t framebuffer;		// physical address of the linear frame buffer; write here to draw to the screen
	uint32_t off_screen_mem_off;
	uint16_t off_screen_mem_size;	// size of memory in the framebuffer but not being displayed on the screen
} __attribute__ ((packed));

static struct vbe_info* vbe_info()
{
    struct vbe_info* vbeinfo = (struct vbe_info*)LOWMEM_START;
    memset(vbeinfo, 0, sizeof(struct vbe_info));
    memcpy(vbeinfo->signature, "VBE2", 4);

    struct int10_regs regs = {
        .eax = 0x4F00,
        .es = 0,
        .edi = (uint32_t)vbeinfo
    };
    int10(&regs);

    int success = regs.eax & 0xFF;
    int status = (regs.eax >> 8) & 0xFF;

    if(success != 0x4F)
        return NULL;

    return vbeinfo;
}

static struct vbe_modeinfo* vbe_modeinfo(int mode)
{
    struct vbe_modeinfo* result = (struct vbe_modeinfo*)LOWMEM_START;
    struct int10_regs regs = {
        .eax = 0x4F01,
        .ecx = mode,
        .es = 0,
        .edi = (uint32_t)result,
    };
    int10(&regs);

    int success = regs.eax & 0xFF;
    int status = (regs.eax >> 8) & 0xFF;

    if(success != 0x4f)
        return NULL;

    return result;
}

static int vbe_setmode(int mode)
{
    static const int MODE_MASK = 0x3FFF;
    static const int MODE_LFB = 1 << 14;
    struct int10_regs regs = {
        .eax = 0x4F02,
        .ebx = (mode & MODE_MASK) | MODE_LFB
    };
    assert(regs.es == 0);
    assert(regs.edi == 0);
    int10(&regs);

    if(regs.eax != 0x004F)
        return 1;
    return 0;
}

static int vbe_current_mode()
{
    struct int10_regs regs = {
        .eax = 0x4F03
    };
    int10(&regs);

    if(regs.eax != 0x4F00)
        return 1;

    return regs.ebx;
}

/*********************************************************************
 * SVGA funcs
 *********************************************************************/
struct svga_mode {
    int code;
    int valid;
    int pitch;
    int supported;
    int lfb;
    int graphics;
    int crtc;
    int width;
    int height;
    int bpp;
    int memory_model;
    int red_position;
    int red_mask;
    int green_position;
    int green_mask;
    int blue_position;
    int blue_mask;
    uint32_t fb;
};

struct svga_info {
    struct svga_mode* modes;
    int modecount;
    size_t memsize;
    int current_mode;   /* offset of into modes */
};

static struct svga_info* svga_info()
{
    struct svga_info* info = kmalloc(sizeof(struct svga_info));

    /* Get VBE info */
    struct vbe_info* vbeinfo = vbe_info();
    if(!vbeinfo) {
        kfree(info);
        return NULL;
    }

    info->memsize = 65536 * vbeinfo->memsize64k;

    /* Copy modes into memory as other vbe calls might overwrite it */
    int modecount = 0;
    uint16_t* modes_ptr = (uint16_t*)((uint32_t)vbeinfo->modes[0] | ((uint32_t)vbeinfo->modes[1] << 16));
    for(uint16_t* ptr = modes_ptr; *ptr != 0xFFFF; ptr++, modecount++);

    uint16_t* modes_buffer = kmalloc(modecount * sizeof(uint16_t));
    memcpy(modes_buffer, modes_ptr, sizeof(uint16_t) * modecount);

    info->modes = kmalloc(sizeof(struct svga_mode) * modecount);
    info->modecount = modecount;

    int vbemode = vbe_current_mode();

    for(int i = 0; i < modecount; i++) {
        int mode = modes_buffer[i];
        assert(mode != 0xFFFF);

        if((mode & 0x1FFF) == (vbemode & 0x1FFF)) {
            info->current_mode = vbemode;
        }

        struct svga_mode* modeinfo = info->modes + i;
        modeinfo->code = mode;

        struct vbe_modeinfo* vbemodeinfo = vbe_modeinfo(mode);
        if(!vbemodeinfo) {
            modeinfo->valid = 0;
        } else {
            modeinfo->valid = 1;
            modeinfo->pitch = vbemodeinfo->pitch;
            modeinfo->supported = vbemodeinfo->attributes & 1;
            modeinfo->lfb = vbemodeinfo->attributes & (1 << 7);
            modeinfo->graphics = vbemodeinfo->attributes & (1 << 4);
            modeinfo->crtc = !!(mode & (1 << 11));
            modeinfo->width = vbemodeinfo->width;
            modeinfo->height = vbemodeinfo->height;
            modeinfo->bpp = vbemodeinfo->bpp;
            modeinfo->memory_model = vbemodeinfo->memory_model;
            modeinfo->fb = vbemodeinfo->framebuffer;
            modeinfo->red_position = vbemodeinfo->red_position;
            modeinfo->red_mask = vbemodeinfo->red_mask;
            modeinfo->green_position = vbemodeinfo->green_position;
            modeinfo->green_mask = vbemodeinfo->green_mask;
            modeinfo->blue_position = vbemodeinfo->blue_position;
            modeinfo->blue_mask = vbemodeinfo->blue_mask;
        }
    }

    return info;
}

static int svga_findmode(const struct svga_info* info, 
                         int width, int height, int bpp)
{
    int selected_mode = -1;
    int pitch;
    void* framebuffer;
    struct vbe_modeinfo svga_mode;

    for(int i = 0; i < info->modecount; i++) {
        const struct svga_mode* mode = info->modes + i;
        if(mode->valid &&
           mode->supported && 
           mode->graphics && 
           mode->lfb && 
           !mode->crtc) {
            if(mode->width == width &&
               mode->height == height &&
               mode->bpp == bpp) {
                if(mode->red_position == 16 &&
                   mode->green_position == 8 &&
                   mode->blue_position == 0) {
                    return i;
                }
            }
        }
    }
    return -1;
}

static int svga_setmode(struct svga_info* info, int mode_idx)
{
    struct svga_mode* mode = info->modes + mode_idx;
    int ret = vbe_setmode(mode->code);
    if(ret)
        return ret;
    info->current_mode = mode_idx;
    return 0;
}


static void svga_putpixel(const struct svga_info* info,
                          int x, int y, uint32_t col)
{
    struct svga_mode* mode = info->modes + info->current_mode;

    if(x >= 0 && x < mode->width) {
        if(y >= 0 && y < mode->height) {
            int offset = (y * mode->pitch) + (x * 4);
            *((uint32_t*)(SVGA_FB + offset)) = col;
        }
    }
}

static void svga_drawhline(struct svga_info* info,
                           int x1, int x2, int y, uint32_t color)
{
    if(x1 > x2)
        swap(x1, x2);

    struct svga_mode* mode = info->modes + info->current_mode;
    clamp(x1, 0, mode->width - 1);
    clamp(x2, 0, mode->width - 1);
    clamp(y, 0, mode->height - 1);

    int offset = (y * mode->pitch) + (x1 * 4);
    uint32_t* ptr = (uint32_t*)(SVGA_FB + offset);
    for(int i = x1; i <= x2; i++) {
        *ptr = color;
        ptr++;
    }
}

static void svga_drawvline(struct svga_info* info,
                           int x, int y1, int y2, uint32_t color)
{
    if(y1 > y2)
        swap(y1, y2);

    struct svga_mode* mode = info->modes + info->current_mode;
    clamp(x, 0, mode->width - 1);
    clamp(y1, 0, mode->height - 1);
    clamp(y2, 0, mode->height - 1);

    int offset = (y1 * mode->pitch) + (x * 4);
    unsigned char* ptr = (unsigned char*)(SVGA_FB + offset);
    for(int i = y1; i <= y2; i++) {
        *((uint32_t*)ptr) = color;
        ptr += mode->pitch;
    }
}

static void svga_drawline(struct svga_info* info,
                          int x1, int y1, int x2, int y2, uint32_t color)
{
    if(x1 == x2) {
        svga_drawvline(info, x1, y1, y2, color);
        return;
    } else if(y1 == y2) {
        svga_drawhline(info, x1, x2, y2, color);
        return;
    }

    struct svga_mode* mode = info->modes + info->current_mode;
    clamp(x1, 0, mode->width);
    clamp(x2, 0, mode->width);
    clamp(y1, 0, mode->height);
    clamp(y2, 0, mode->height);

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

    svga_putpixel(info, px, py, color);

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
            svga_putpixel(info, px,py,color);
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
            svga_putpixel(info, px,py,color);
        }
    }

}

static void svga_drawpolygon(struct svga_info* info,
                             int nvert, const int* vert, uint32_t color)
{
    for(int i = 0; i < nvert - 1; i++) {
        svga_drawline(info,
                      vert[i * 2], vert[(i * 2) + 1],
                      vert[(i + 1) * 2], vert[((i + 1) * 2) + 1],
                      color);
    }
    svga_drawline(info,
                  vert[0], vert[1],
                  vert[(nvert - 1) * 2], vert[((nvert - 1) * 2) + 1], 
                  color);
}

static void svga_drawrect(struct svga_info* info,
                          int left, int top, int right, int bottom, uint32_t color)
{
    svga_drawline(info, left, top, right, top, color);
    svga_drawline(info, right, top, right, bottom, color);
    svga_drawline(info, right, bottom, left, bottom, color);
    svga_drawline(info, left, bottom, left, top, color);
}

static void svga_fillrect(struct svga_info* info,
                          int left, int top, int right, int bottom, uint32_t color)
{
    if(top > bottom)
        swap(top, bottom);
    if(left > right)
        swap(left, right);

    struct svga_mode* mode = info->modes + info->current_mode;
    clamp(left, 0, mode->width);
    clamp(top, 0, mode->height);
    clamp(right, 0, mode->width);
    clamp(bottom, 0, mode->height);

    int top_offset = (top * mode->pitch) + (left * 4);
    int bottom_offset = (bottom * mode->pitch) + left;
    int width = right - left + 1;

    for(int i = top_offset; i <= bottom_offset; i += mode->pitch) {
        uint32_t* ptr = (uint32_t*)(SVGA_FB + i);
        for(int j = 0; j < width; j++) {
            *ptr = color;
            ptr++;
        }
    }
}

static void svga_drawcircle(struct svga_info* info,
                            int cx, int cy, int radius, uint32_t color)
{
    if(radius == 0)
        return;

    struct svga_mode* mode = info->modes + info->current_mode;

    /* midpoint circle algorithm */
#define put4(cx, cy, x, y, color) \
    do { \
		svga_putpixel(info, (cx) + (x), (cy) + (y), color); \
		svga_putpixel(info, (cx) - (x), (cy) + (y), color); \
		svga_putpixel(info, (cx) + (x), (cy) - (y), color); \
		svga_putpixel(info, (cx) - (x), (cy) - (y), color); \
    } while(0)
#define put8(cx, cy, x, y, color) \
    do { \
        put4(cx, cy, x, y, color); \
        put4(cx, cy, y, x, color); \
    } while(0)

	int error = -radius;
	int x = radius;
	int y = 0;

	while (x >= y) {
		put8(cx, cy, x, y, color);

		error += y;
		y++;
		error += y;

		if (error >= 0) {
			error += -x;
			x--;
			error += -x;
		}
	}
#undef put8
#undef put4
}

static void svga_fillcircle(struct svga_info* info,
                            int cx, int cy, int radius, uint32_t color)
{
    if(radius == 0)
        return;

    struct svga_mode* mode = info->modes + info->current_mode;

    /* midpoint circle algorithm */
#define put4(cx, cy, x, y, color) \
    do { \
        svga_drawhline(info, cx + x, cx - x, cy + y, color); \
        svga_drawhline(info, cx + x, cx - x, cy - y, color); \
    } while(0)
#define put8(cx, cy, x, y, color) \
    do { \
        put4(cx, cy, x, y, color); \
        put4(cx, cy, y, x, color); \
    } while(0)

	int error = -radius;
	int x = radius;
	int y = 0;

	while (x >= y) {
		put8(cx, cy, x, y, color);

		error += y;
		y++;
		error += y;

		if (error >= 0) {
			error += -x;
			x--;
			error += -x;
		}
	}
#undef put8
#undef put4

}

static int random(int lo, int max)
{
    int rnd = xorshift32(&rng_state);
    if(rnd < 0)
        rnd = -rnd;
    int delta = max - lo + 1;
    int ret = (rnd % delta) + lo;
    return ret;
}

void test_int10()
{
    trace("Testing int10 calls");

    /* Initialize RNG */
    rng_state = rdtsc() & 0xFFFFFFFF;

    /* Map conventional memory in because we need it */
    for(int i = 0; i <= 0x000FFFFF; i += 4096) {
        vmm_map((void*)i, i, VMM_PAGE_PRESENT|VMM_PAGE_WRITABLE);
    }

    /* Copy stub to 0x7C00 */
    memcpy((void*)0x7C00, obj_int10_stub_bin, sizeof(obj_int10_stub_bin));

    /* Get svga info */
    struct svga_info* svgainfo = svga_info();
    assert(svgainfo != NULL);

    /* Find suitable mode */
    int mode_idx = svga_findmode(svgainfo, 640, 480, 32);
    assert(mode_idx != -1);

    struct svga_mode* mode = svgainfo->modes + mode_idx;

    /* Switch mode */
    int ret = svga_setmode(svgainfo, mode_idx);
    assert(ret == 0);

    // Map framebuffer
    unsigned char* fb = (unsigned char*)SVGA_FB;
    for(unsigned char* va = (unsigned char*)SVGA_FB, 
        *pa = (unsigned char*)mode->fb;
        va <= fb + svgainfo->memsize;
        va += 4096, pa += 4096) {
        vmm_map(va, (uint32_t)pa, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
    }

    /* Draw effects */
    for(int i = 0; i < 10; i++) {
        svga_putpixel(svgainfo,
                      random(0, 639),
                      random(0, 479),
                      random(0, 0xFFFFFF));
        delay();
    }

    for(int i = 0; i < 10; i++) {
        svga_drawhline(svgainfo, random(0, 639), random(0, 639), random(0, 479), random(0, 0xFFFFFF));
        svga_drawvline(svgainfo, random(0, 639), random(0, 639), random(0, 479), random(0, 0xFFFFFF));
        delay();
    }


    for(int i = 0; i < 10; i++) {
        int x1 = random(0, 639);
        int x2 = random(0, 639);
        int y1 = random(0, 479);
        int y2 = random(0, 479);

        int r = random(0, 255);
        int g = random(0, 255);
        int b = random(0, 255);
        uint32_t col = (r << 16) | (g << 8) | b;

        svga_drawline(svgainfo, x1, y1, x2, y2, col);
        delay();
    }

    for(int i = 0; i < 10; i++) {
        int vertices[6];
        vertices[0] = random(0, 639);
        vertices[1] = random(0, 479);

        vertices[2] = random(0, 639);
        vertices[3] = random(0, 479);

        vertices[4] = random(0, 639);
        vertices[5] = random(0, 479);

        svga_drawpolygon(svgainfo, 3, vertices, random(0, 0xFFFFFF));
        delay();
    }

    for(int i = 0; i < 10; i++) {
        int left = random(0, 639);
        int top = random(0, 479);
        int right = random(0, 639);
        int bottom = random(0, 479);

        svga_drawrect(svgainfo, left, top, right, bottom, random(0, 0xFFFFFF));
        delay();
    }

    for(int i = 0; i < 10; i++) {
        int left = random(0, 639);
        int top = random(0, 479);
        int right = random(0, 639);
        int bottom = random(0, 479);

        svga_fillrect(svgainfo, left, top, right, bottom, random(0, 0xFFFFFF));
        delay();
    }

    for(int i = 0; i < 10; i++) {
        svga_drawcircle(svgainfo,
                        random(0, 639),
                        random(0, 479),
                        random(10, 480 / 3),
                        random(0, 0xFFFFFF));
        delay();
    }

    for(int i = 0; i < 10; i++) {
        svga_fillcircle(svgainfo,
                        random(0, 639),
                        random(0, 479),
                        random(10, 480 / 3),
                        random(0, 0xFFFFFF));
        delay();
    }
}


