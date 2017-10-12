#include <runtime.h>
#include <port.h>
#include <malloc.h>
#include <debug.h>
#include <string.h>
#include <random.h>
#include <util.h>
#include "vbe.h"
#include "svga.h"
#include "bmp.h"

#define FB_BASE 0x1000000

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


struct svga_info* svga_info()
{
    struct svga_info* info = malloc(sizeof(struct svga_info));

    /* Get VBE info */
    struct vbe_info* vbeinfo = vbe_info();
    if(!vbeinfo) {
        free(info);
        return NULL;
    }

    info->memsize = 65536 * vbeinfo->memsize64k;

    /* Copy modes into memory as other vbe calls might overwrite it */
    int modecount = 0;
    uint16_t* modes_ptr = (uint16_t*)
        (((uint32_t)vbeinfo->modes.seg << 16) | ((uint32_t)vbeinfo->modes.off));
    for(uint16_t* ptr = modes_ptr; *ptr != 0xFFFF; ptr++, modecount++);

    uint16_t* modes_buffer = malloc(modecount * sizeof(uint16_t));
    memcpy(modes_buffer, modes_ptr, sizeof(uint16_t) * modecount);

    info->modes = malloc(sizeof(struct svga_mode) * modecount);
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
            modeinfo->fb_ptr = 0;
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

int svga_findmode(const struct svga_info* info, 
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

int svga_setmode(struct svga_info* info, int mode_idx)
{
    struct svga_mode* mode = info->modes + mode_idx;
    int ret = vbe_setmode(mode->code);
    if(ret)
        return ret;
    info->current_mode = mode_idx;
    mode->fb_ptr = (unsigned char*)FB_BASE;
    return 0;
}


void svga_putpixel(const struct svga_info* info,
                   int x, int y, uint32_t col)
{
    struct svga_mode* mode = info->modes + info->current_mode;

    if(x >= 0 && x < mode->width) {
        if(y >= 0 && y < mode->height) {
            int offset = (y * mode->pitch) + (x * (mode->bpp / 8));
            *((uint32_t*)(mode->fb_ptr + offset)) = col;
        }
    }
}

void svga_drawhline(const struct svga_info* info,
                    int x1, int x2, int y, uint32_t color)
{
    if(x1 > x2)
        swap(x1, x2);

    struct svga_mode* mode = info->modes + info->current_mode;
    clamp(x1, 0, mode->width - 1);
    clamp(x2, 0, mode->width - 1);
    clamp(y, 0, mode->height - 1);

    int offset = (y * mode->pitch) + (x1 * (mode->bpp / 8));
    uint32_t* ptr = (uint32_t*)(mode->fb_ptr + offset);
    for(int i = x1; i <= x2; i++) {
        *ptr = color;
        ptr++;
    }
}

void svga_drawvline(const struct svga_info* info,
                    int x, int y1, int y2, uint32_t color)
{
    if(y1 > y2)
        swap(y1, y2);

    struct svga_mode* mode = info->modes + info->current_mode;
    clamp(x, 0, mode->width - 1);
    clamp(y1, 0, mode->height - 1);
    clamp(y2, 0, mode->height - 1);

    int offset = (y1 * mode->pitch) + (x * (mode->bpp / 8));
    unsigned char* ptr = (unsigned char*)(mode->fb_ptr + offset);
    for(int i = y1; i <= y2; i++) {
        *((uint32_t*)ptr) = color;
        ptr += mode->pitch;
    }
}

void svga_drawline(const struct svga_info* info,
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
            svga_putpixel(info, px, py, color);
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
            svga_putpixel(info, px, py, color);
        }
    }

}

void svga_drawpolygon(const struct svga_info* info,
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

void svga_drawrect(const struct svga_info* info,
                   int left, int top, int right, int bottom, uint32_t color)
{
    svga_drawline(info, left, top, right, top, color);
    svga_drawline(info, right, top, right, bottom, color);
    svga_drawline(info, right, bottom, left, bottom, color);
    svga_drawline(info, left, bottom, left, top, color);
}

void svga_fillrect(const struct svga_info* info,
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

    int top_offset = (top * mode->pitch) + (left * (mode->bpp / 8));
    int bottom_offset = (bottom * mode->pitch) + left;
    int width = right - left + 1;

    for(int i = top_offset; i <= bottom_offset; i += mode->pitch) {
        uint32_t* ptr = (uint32_t*)(mode->fb_ptr + i);
        for(int j = 0; j < width; j++) {
            *ptr = color;
            ptr++;
        }
    }
}

void svga_drawcircle(struct svga_info* info,
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

void svga_fillcircle(struct svga_info* info,
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

static uint32_t rng_state;

static int random(int lo, int max)
{
    int rnd = xorshift32(&rng_state);
    if(rnd < 0)
        rnd = -rnd;
    int delta = max - lo + 1;
    int ret = (rnd % delta) + lo;
    return ret;
}

static size_t load_bitmap(const char* filename, void** out_buffer)
{
    int fd = open(filename, O_RDONLY, 0);
    assert(fd != -1);

    size_t buffer_size = 65536;
    size_t total_read = 0;
    void* buffer = malloc(buffer_size);
    unsigned char* ptr = buffer;
    while(1) {
        if(total_read + 512 > buffer_size) {
            buffer_size += 65536;
            buffer = realloc(buffer, buffer_size);
            ptr = ((unsigned char*)buffer) + total_read;
        }

        int read_bytes = read(fd, ptr, 512);
        if(read_bytes == -1) {
            panic("Read error");
        } else if(read_bytes == 0) {
            break;
        }

        ptr += read_bytes;
        total_read += read_bytes;
    }

    *out_buffer = buffer;
    return total_read;
}

int main()
{
    trace("svga driver started");

    /* Init RNG */
    trace("init rng");
    rng_state = rdtsc() & 0xFFFFFFFF;

    /* Map lowmem */
    trace("map lowmem");
    int ret = mmap_phys(LOWMEM_START, 
                        (void*)LOWMEM_START, 
                        0x100000 - LOWMEM_START, 
                        PROT_READ|PROT_WRITE);
    if(ret) {
        panic("mmap_phys failed: %d", ret);
    }

    /* Get svga info */
    trace("get svga info");
    struct svga_info* svgainfo = svga_info();
    assert(svgainfo != NULL);

    /* Find suitable mode */
    trace("find mode");
    int mode_idx = svga_findmode(svgainfo, 640, 480, 32);
    assert(mode_idx != -1);

    struct svga_mode* mode = svgainfo->modes + mode_idx;

    /* Switch mode */
    trace("switch mode");
    ret = svga_setmode(svgainfo, mode_idx);
    assert(ret == 0);

    // Map framebuffer
    trace("map framebuffer");
    trace("memsize: %dK", svgainfo->memsize / 1024);
    trace("fb: %p - %p", mode->fb_ptr, mode->fb_ptr + svgainfo->memsize);
    ret = mmap_phys(mode->fb, mode->fb_ptr, svgainfo->memsize, PROT_READ|PROT_WRITE);
    if(ret) {
        panic("mmap_phys failed: %d", ret);
    }

    /* Load bitmap into memory */
    trace("Loading bitmap");
    void* bmp_buffer;
    size_t bmp_size = load_bitmap("burleigh.bmp", &bmp_buffer);

    struct bmpfileheader* fhdr = bmp_buffer;
    assert(fhdr->signature == 0x4D42);

    struct bmpinfoheader* ihdr = (struct bmpinfoheader*)(((unsigned char*)bmp_buffer) + sizeof(struct bmpfileheader));
    assert(ihdr->size >= sizeof(struct bmpinfoheader));
    assert(ihdr->planes == 1);
    assert(ihdr->bpp == 24);
    assert(ihdr->compression == 0);
    assert(ihdr->ncols == 0);

    int bmp_pitch = (((ihdr->bpp * ihdr->width) + 31) / 32) * 4;
    unsigned char* bmp_data = (unsigned char*)bmp_buffer + fhdr->offset;
    for(int row = 0; row < ihdr->height; row++) {
        unsigned char* row_data = bmp_data + ((ihdr->height - row) * bmp_pitch);
        unsigned char* row_ptr = row_data;
        for(int col = 0; col < ihdr->width; col++) {
            int b = *row_ptr++;
            int g = *row_ptr++;
            int r = *row_ptr++;
            uint32_t color = (r << 16) | (g << 8) | b;
            svga_putpixel(svgainfo, col, row, color);
        }
    }
    while(1);

    return 0;
}


