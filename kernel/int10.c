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

/* Start of conventional memory (29Kb) */
#define LOWMEM_START    0x500

struct svga_info {
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

struct svga_modeinfo {
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

static int svga_info(struct svga_info* info)
{
    memcpy((void*)LOWMEM_START, info, sizeof(struct svga_info));

    struct int10_regs regs = {
        .eax = 0x4F00,
        .es = 0,
        .edi = LOWMEM_START
    };
    int10(&regs);

    int success = regs.eax & 0xFF;
    int status = (regs.eax >> 8) & 0xFF;

    if(success != 0x4F)
        return 1;

    memcpy(info, (void*)LOWMEM_START, sizeof(struct svga_info));
    return status;
}


static int svga_modeinfo(int mode, struct svga_modeinfo* info)
{
    struct int10_regs regs = {
        .eax = 0x4F01,
        .ecx = mode,
        .es = 0,
        .edi = LOWMEM_START,
    };
    int10(&regs);

    int success = regs.eax & 0xFF;
    int status = (regs.eax >> 8) & 0xFF;

    if(success != 0x4f)
        return 1;

    memcpy(info, (void*)LOWMEM_START, sizeof(struct svga_modeinfo));
    return 0;
}

static int svga_setmode(int mode)
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

    /* Get vbe info */
    struct svga_info info = { 0 };
    memcpy(info.signature, "VBE2", 4);

    int selected_mode = -1;
    int pitch;
    void* framebuffer;
    struct svga_modeinfo svga_mode;

    int ret = svga_info(&info);
    if(ret == 0) {
        trace("svga info:");
        if(!memcmp((void*)info.signature, "VBE2", 4)) {
            trace("    vbe2 supported");
        } else if(!memcmp((void*)info.signature, "VESA", 4)) {
            trace("    vesa supported");
        } else {
            trace("    vesa/vbe2 not supported");
        }

        int ver_minor = info.version & 0xFF;
        int ver_major = (info.version >> 8) & 0xFF;
        trace("    vbe version: %d.%d", ver_major, ver_minor);
        trace("    supported modes:");

        /* 
         * Must copy modes to scratch memory beforehand as 
         * the buffer is reused between int10 calls 
         */
        uint16_t* modes = kmalloc(sizeof(uint32_t)*1024); /* 1024 modes should be enough for everyone */
        uint16_t* modes_src = (uint16_t*)((uint32_t)info.modes[0] | ((uint32_t)info.modes[1] << 16));
        assert(modes_src == (uint16_t*)0x522);
        int mode_count = 0;
        for(; mode_count < 1024 && (*modes_src != 0xFFFF); mode_count++) {
            modes[mode_count] = *modes_src;
            modes_src++;
        }
        modes[mode_count] = 0xFFFF;

        uint16_t* mode = modes;
        while(*mode != 0xFFFF) {
            struct svga_modeinfo modeinfo;
            ret = svga_modeinfo(*mode, &modeinfo);
            if(ret == 0) {
                int supported = modeinfo.attributes & 1;
                int lfb = modeinfo.attributes & (1 << 7);
                int graphics = modeinfo.attributes & (1 << 4);
                int crtc =  !!(*mode & (1 << 11));
                if(supported && graphics && lfb) {
                    trace("        0x%04X: %dx%dx%d lfb: %p crtc: %d model: %d",
                          (int)*mode,
                          (int)modeinfo.width,
                          (int)modeinfo.height,
                          (int)modeinfo.bpp,
                          modeinfo.framebuffer,
                          crtc,
                          modeinfo.memory_model);
                    if(modeinfo.width == 640 && modeinfo.height == 480 &&
                       modeinfo.bpp == 32 && lfb && !crtc && 
                       (modeinfo.memory_model == 4 || modeinfo.memory_model == 6)) {
                        selected_mode = *mode;
                        svga_mode = modeinfo;
                        pitch = modeinfo.pitch;
                        framebuffer = (void*)modeinfo.framebuffer;
                    }
                }
            } 

            mode++;
        }

        if(selected_mode == -1) {
            panic("No 640x480x32 mode found!");
        }

        selected_mode = selected_mode | (1 << 14);
        trace("Selected mode: 0x%X", selected_mode);
        trace("Positions: R: %d, G: %d, B: %d",
              (int)svga_mode.red_position,
              (int)svga_mode.green_position,
              (int)svga_mode.blue_position);
        
        ret = svga_setmode(selected_mode);
        if(ret) {
            panic("Failed to set video mode");
        }
    }

    // Map framebuffer
    unsigned char* fb = (unsigned char*)0x400000;
    size_t fb_size = info.memsize64k * 65536;
    trace("fb_size: %d Kb", fb_size / 1024);
    for(unsigned char* va = (unsigned char*)0x400000, *pa = framebuffer;
        va <= fb + fb_size;
        va += 4096, pa += 4096) {
        vmm_map(va, (uint32_t)pa, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
    }

    // uint32 pixel_offset = y * pitch + (x * (bpp/8)) + framebuffer;
    int x = 640/2;
    int y = 480/2;
    int offset = (y * pitch) + (x * 4);
    int r = 0x7F, g = 0x7F, b = 0x7F;

    *((uint32_t*)(fb + offset)) = (r << svga_mode.red_position) |
                                  (g << svga_mode.green_position) |
                                  (b << svga_mode.blue_position);
#if 0
    for(int i = 0; i < 1000; i++) {
        int x = random(0, 639);
        int y = random(0, 479);
        int col = random(0, 255);

        int offset = (y * pitch) + x;
        fb[offset] = col & 0xFF;
    }
#endif
}


