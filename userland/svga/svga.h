#pragma once

#include <stdint.h>

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
    uintptr_t fb;
    unsigned char* fb_ptr;
};

struct svga_info {
    struct svga_mode* modes;
    int modecount;
    size_t memsize;
    int current_mode;   /* offset of into modes */
};

struct svga_info* svga_info();
int svga_findmode(const struct svga_info* info, 
                  int width, int height, int bpp);
int svga_setmode(struct svga_info* info, int mode_idx);
void svga_putpixel(const struct svga_info* info,
                   int x, int y, uint32_t col);
void svga_drawhline(const struct svga_info* info,
                    int x1, int x2, int y, uint32_t color);
void svga_drawvline(const struct svga_info* info,
                    int x, int y1, int y2, uint32_t color);
void svga_drawline(const struct svga_info* info,
                   int x1, int y1, int x2, int y2, uint32_t color);
void svga_drawpolygon(const struct svga_info* info,
                      int nvert, const int* vert, uint32_t color);
void svga_drawrect(const struct svga_info* info,
                   int left, int top, int right, int bottom, uint32_t color);
void svga_drawrect(const struct svga_info* info,
                   int left, int top, int right, int bottom, uint32_t color);
void svga_fillrect(const struct svga_info* info,
                   int left, int top, int right, int bottom, uint32_t color);
void svga_drawcircle(struct svga_info* info,
                     int cx, int cy, int radius, uint32_t color);
void svga_fillcircle(struct svga_info* info,
                     int cx, int cy, int radius, uint32_t color);


