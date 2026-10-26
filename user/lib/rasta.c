#include "rasta.h"
#include "../../kernel/syscall.h"
#include <string.h>

uint64_t sys_getticks(void)
{
    int64_t ticks = syscall64(SYSCALL_GETTICKS, 0, 0, 0, 0, 0);
    return ticks;
}

void sys_sleep(uint32_t ticks)
{
    syscall(SYSCALL_SLEEP, ticks, 0, 0, 0, 0);
}

void sys_exit(uint8_t status)
{
    syscall(SYSCALL_EXIT, status, 0, 0, 0, 0);
    __builtin_unreachable();
}

void sys_exit_thread(void)
{
    syscall(SYSCALL_EXIT_THREAD, 0, 0, 0, 0, 0);
    __builtin_unreachable();
}

int sys_get_kbd_event(struct KeyEvent* evt, int* overflow, int block)
{
    return syscall(SYSCALL_GET_KBD_EVENT, (uint32_t)evt, (uintptr_t)overflow,
                   block, 0, 0);
}

int sys_write(const char* s, size_t len)
{
    return syscall(SYSCALL_WRITE, (uintptr_t)s, len, 0, 0, 0);
}

int sys_exec(const char* command, const char* arguments)
{
    return syscall(SYSCALL_EXEC, (uintptr_t)command, (uintptr_t)arguments, 0,
                   0, 0);
}

int sys_shutdown(void)
{
    return syscall(SYSCALL_SHUTDOWN, 0, 0, 0, 0, 0);
}

int sys_list(void* buffer, size_t len)
{
    return syscall(SYSCALL_LIST, (uintptr_t)buffer, len, 0, 0, 0);
}

int sys_lock_screen(void)
{
    return syscall(SYSCALL_LOCK_SCREEN, 0, 0, 0, 0, 0);
}

int sys_unlock_screen(void)
{
    return syscall(SYSCALL_UNLOCK_SCREEN, 0, 0, 0, 0, 0);
}

int sys_trace(const char* file, size_t file_len, int line, const char* s,
              size_t len)
{
    return syscall(SYSCALL_TRACE, (uintptr_t)file, file_len, line,
                   (uintptr_t)s, len);
}

int sys_vga_swap(void)
{
    return syscall(SYSCALL_VGA_SWAP, 0, 0, 0, 0, 0);
}

int sys_vga_set_palette(uint8_t index, uint8_t red, uint8_t green,
                        uint8_t blue)
{
    return syscall(SYSCALL_VGA_SET_PALETTE, index, red, green, blue, 0);
}

#define MODE_X_STRIDE VGA_MODE_X_WIDTH
static uint8_t* backbuffer = (uint8_t*)VGA_PHYSICAL_START;
static struct Bitmap screen = {VGA_MODE_X_WIDTH, VGA_MODE_X_HEIGHT,
                               (uint8_t*)VGA_PHYSICAL_START};

static void order_bounds(int* a, int* b)
{
    if (*a > *b) {
        int tmp = *a;
        *a = *b;
        *b = tmp;
    }
}

static bool bounds_visible(int x1, int y1, int x2, int y2)
{
    return x1 >= 0 && y1 >= 0 && x2 < VGA_MODE_X_WIDTH &&
           y2 < VGA_MODE_X_HEIGHT;
}

static int column_offset(unsigned x)
{
    return x;
}

static void horizontal_span_unchecked(int x1, int y, int x2, uint8_t color)
{
    memset(backbuffer + y * MODE_X_STRIDE + x1, color, x2 - x1 + 1);
}

static void horizontal_span(int x1, int y, int x2, uint8_t color)
{
    if ((unsigned)y >= VGA_MODE_X_HEIGHT)
        return;
    if (x1 < 0)
        x1 = 0;
    if (x2 >= VGA_MODE_X_WIDTH)
        x2 = VGA_MODE_X_WIDTH - 1;
    if (x1 > x2)
        return;

    horizontal_span_unchecked(x1, y, x2, color);
}

static void vertical_span_unchecked(int x, int y1, int y2, uint8_t color)
{
    unsigned offset = column_offset(x) + y1 * MODE_X_STRIDE;
    unsigned rows = y2 - y1 + 1;
    do {
        backbuffer[offset] = color;
        offset += MODE_X_STRIDE;
    } while (--rows);
}

static void vertical_span(int x, int y1, int y2, uint8_t color)
{
    if ((unsigned)x >= VGA_MODE_X_WIDTH)
        return;
    if (y1 < 0)
        y1 = 0;
    if (y2 >= VGA_MODE_X_HEIGHT)
        y2 = VGA_MODE_X_HEIGHT - 1;
    if (y1 > y2)
        return;

    vertical_span_unchecked(x, y1, y2, color);
}

void vga_putpixel(unsigned x, unsigned y, uint8_t color)
{
    if (x >= VGA_MODE_X_WIDTH || y >= VGA_MODE_X_HEIGHT) {
        return;
    }

    backbuffer[y * MODE_X_STRIDE + x] = color;
}

void vga_clear(uint8_t color)
{
    memset(backbuffer, color, VGA_MODE_X_WIDTH * VGA_MODE_X_HEIGHT);
}

void vga_fillrect(int x1, int y1, int x2, int y2, uint8_t color)
{
    order_bounds(&x1, &x2);
    order_bounds(&y1, &y2);
    if (y1 == y2) {
        horizontal_span(x1, y1, x2, color);
        return;
    }
    if (x1 == x2) {
        vertical_span(x1, y1, y2, color);
        return;
    }
    if (x1 < 0)
        x1 = 0;
    if (y1 < 0)
        y1 = 0;
    if (x2 >= VGA_MODE_X_WIDTH)
        x2 = VGA_MODE_X_WIDTH - 1;
    if (y2 >= VGA_MODE_X_HEIGHT)
        y2 = VGA_MODE_X_HEIGHT - 1;
    if (x1 > x2 || y1 > y2)
        return;

    size_t count = x2 - x1 + 1;
    uint8_t* dst = backbuffer + y1 * MODE_X_STRIDE + x1;
    if (count == MODE_X_STRIDE) {
        memset(dst, color, count * (y2 - y1 + 1));
    } else {
        for (int y = y1; y <= y2; ++y, dst += MODE_X_STRIDE)
            memset(dst, color, count);
    }
}

void vga_rect(int x1, int y1, int x2, int y2, uint8_t color)
{
    order_bounds(&x1, &x2);
    order_bounds(&y1, &y2);
    if (bounds_visible(x1, y1, x2, y2)) {
        if (x1 == x2) {
            vertical_span_unchecked(x1, y1, y2, color);
            return;
        }
        horizontal_span_unchecked(x1, y1, x2, color);
        if (y1 == y2)
            return;
        horizontal_span_unchecked(x1, y2, x2, color);
        ++y1;
        --y2;
        if (y1 <= y2) {
            vertical_span_unchecked(x1, y1, y2, color);
            vertical_span_unchecked(x2, y1, y2, color);
        }
        return;
    }
    if (x1 == x2) {
        vertical_span(x1, y1, y2, color);
        return;
    }
    horizontal_span(x1, y1, x2, color);
    if (y1 == y2)
        return;
    horizontal_span(x1, y2, x2, color);
    ++y1;
    --y2;
    if (y1 <= y2) {
        vertical_span(x1, y1, y2, color);
        vertical_span(x2, y1, y2, color);
    }
}

static void drawline_unchecked(int x1, int y1, int x2, int y2, uint8_t color)
{
    int dx = x2 > x1 ? x2 - x1 : x1 - x2;
    int dy = y2 > y1 ? y2 - y1 : y1 - y2;
    if (dy > dx) {
        if (y1 > y2) {
            order_bounds(&y1, &y2);
            int tmp = x1;
            x1 = x2;
            x2 = tmp;
        }
        int x_step = x1 < x2 ? 1 : -1;
        int column_step = x_step;
        int offset = column_offset(x1) + y1 * MODE_X_STRIDE;
        int error = dy / 2;
        for (int y = y1; y <= y2; ++y) {
            backbuffer[offset] = color;
            offset += MODE_X_STRIDE;
            error += dx;
            if (error >= dy) {
                error -= dy;
                x1 += x_step;
                offset += column_step;
            }
        }
    } else {
        if (x1 > x2) {
            order_bounds(&x1, &x2);
            int tmp = y1;
            y1 = y2;
            y2 = tmp;
        }
        int row_step = y1 < y2 ? MODE_X_STRIDE : -MODE_X_STRIDE;
        int offset = column_offset(x1) + y1 * MODE_X_STRIDE;
        int error = dx / 2;
        for (int x = x1; x <= x2; ++x) {
            backbuffer[offset] = color;
            offset += 1;
            error += dy;
            if (error >= dx) {
                error -= dx;
                offset += row_step;
            }
        }
    }
}

static void drawline32(int x1, int y1, int x2, int y2, uint8_t color)
{
    int dx = x2 - x1;
    int dy = y2 - y1;
    bool steep = (dy < 0 ? -dy : dy) > (dx < 0 ? -dx : dx);
    int major1 = steep ? y1 : x1;
    int major2 = steep ? y2 : x2;
    int minor1 = steep ? x1 : y1;
    int minor2 = steep ? x2 : y2;
    if (major1 > major2) {
        order_bounds(&major1, &major2);
        int tmp = minor1;
        minor1 = minor2;
        minor2 = tmp;
    }
    int limit = steep ? VGA_MODE_X_HEIGHT : VGA_MODE_X_WIDTH;
    int first = major1 < 0 ? 0 : major1;
    int last = major2 >= limit ? limit - 1 : major2;
    if (first > last)
        return;

    unsigned major = major2 - major1;
    int delta = minor2 - minor1;
    int step = delta < 0 ? -1 : 1;
    unsigned minor = delta < 0 ? -delta : delta;
    unsigned error = major / 2;
    int pos = minor1;
    if (first != major1) {
        unsigned phase = (unsigned)(first - major1) * minor + error;
        pos += (int)(phase / major) * step;
        error = phase % major;
    }
    for (int i = first; i <= last; ++i) {
        if (steep)
            vga_putpixel(pos, i, color);
        else
            vga_putpixel(i, pos, color);
        error += minor;
        if (error >= major) {
            error -= major;
            pos += step;
        }
    }
}

static __attribute__((noinline)) void drawline64(int x1, int y1, int x2,
                                                 int y2, uint8_t color)
{
    int64_t dx = (int64_t)x2 - x1;
    int64_t dy = (int64_t)y2 - y1;
    bool steep = (dy < 0 ? -dy : dy) > (dx < 0 ? -dx : dx);
    int major1 = steep ? y1 : x1;
    int major2 = steep ? y2 : x2;
    int minor1 = steep ? x1 : y1;
    int minor2 = steep ? x2 : y2;
    if (major1 > major2) {
        order_bounds(&major1, &major2);
        int tmp = minor1;
        minor1 = minor2;
        minor2 = tmp;
    }
    int limit = steep ? VGA_MODE_X_HEIGHT : VGA_MODE_X_WIDTH;
    int first = major1 < 0 ? 0 : major1;
    int last = major2 >= limit ? limit - 1 : major2;
    if (first > last)
        return;

    uint32_t major = (int64_t)major2 - major1;
    int64_t delta = (int64_t)minor2 - minor1;
    int step = delta < 0 ? -1 : 1;
    uint32_t minor = delta < 0 ? -delta : delta;
    uint64_t phase = (uint64_t)((int64_t)first - major1) * minor + major / 2;
    int64_t pos = minor1 + (int64_t)(phase / major) * step;
    uint64_t error = phase % major;
    for (int i = first; i <= last; ++i) {
        if (steep)
            vga_putpixel(pos, i, color);
        else
            vga_putpixel(i, pos, color);
        error += minor;
        if (error >= major) {
            error -= major;
            pos += step;
        }
    }
}

void vga_drawline(int x1, int y1, int x2, int y2, uint8_t color)
{
    if (y1 == y2) {
        order_bounds(&x1, &x2);
        horizontal_span(x1, y1, x2, color);
        return;
    }
    if (x1 == x2) {
        order_bounds(&y1, &y2);
        vertical_span(x1, y1, y2, color);
        return;
    }

    if ((unsigned)x1 < VGA_MODE_X_WIDTH && (unsigned)y1 < VGA_MODE_X_HEIGHT &&
        (unsigned)x2 < VGA_MODE_X_WIDTH && (unsigned)y2 < VGA_MODE_X_HEIGHT) {
        drawline_unchecked(x1, y1, x2, y2, color);
    } else if ((unsigned)x1 + 32768u <= 65535u &&
               (unsigned)y1 + 32768u <= 65535u &&
               (unsigned)x2 + 32768u <= 65535u &&
               (unsigned)y2 + 32768u <= 65535u) {
        drawline32(x1, y1, x2, y2, color);
    } else {
        drawline64(x1, y1, x2, y2, color);
    }
}

static void oval_unchecked(int x1, int y1, int x2, int y2, uint8_t color)
{
    int a = x2 - x1;
    int b = y2 - y1;
    int odd = b & 1;
    int dx = 4 * (1 - a) * b * b;
    int dy = 4 * (odd + 1) * a * a;
    int error = dx + dy + odd * a * a;
    y1 += (b + 1) / 2;
    y2 = y1 - odd;
    a *= 8 * a;
    odd = 8 * b * b;
    int left = column_offset(x1);
    int right = column_offset(x2);
    int bottom = y1 * MODE_X_STRIDE;
    int top = y2 * MODE_X_STRIDE;

    do {
        backbuffer[right + bottom] = color;
        backbuffer[left + bottom] = color;
        backbuffer[left + top] = color;
        backbuffer[right + top] = color;
        int twice = 2 * error;
        if (twice <= dy) {
            ++y1;
            --y2;
            bottom += MODE_X_STRIDE;
            top -= MODE_X_STRIDE;
            dy += a;
            error += dy;
        }
        if (twice >= dx || 2 * error > dy) {
            ++x1;
            --x2;
            left += 1;
            right += -1;
            dx += odd;
            error += dx;
        }
    } while (x1 <= x2);

    left += -1;
    right += 1;
    while (y1 - y2 <= b) {
        backbuffer[left + bottom] = color;
        backbuffer[right + bottom] = color;
        backbuffer[left + top] = color;
        backbuffer[right + top] = color;
        ++y1;
        --y2;
        bottom += MODE_X_STRIDE;
        top -= MODE_X_STRIDE;
    }
}

static void oval32(int x1, int y1, int x2, int y2, uint8_t color)
{
    int a = x2 - x1;
    int b = y2 - y1;
    int odd = b & 1;
    int dx = 4 * (1 - a) * b * b;
    int dy = 4 * (odd + 1) * a * a;
    int error = dx + dy + odd * a * a;
    y1 += (b + 1) / 2;
    y2 = y1 - odd;
    a *= 8 * a;
    odd = 8 * b * b;

    do {
        vga_putpixel(x2, y1, color);
        vga_putpixel(x1, y1, color);
        vga_putpixel(x1, y2, color);
        vga_putpixel(x2, y2, color);
        int twice = 2 * error;
        if (twice <= dy) {
            ++y1;
            --y2;
            dy += a;
            error += dy;
        }
        if (twice >= dx || 2 * error > dy) {
            ++x1;
            --x2;
            dx += odd;
            error += dx;
        }
    } while (x1 <= x2);

    while (y1 - y2 <= b) {
        vga_putpixel(x1 - 1, y1, color);
        vga_putpixel(x2 + 1, y1++, color);
        vga_putpixel(x1 - 1, y2, color);
        vga_putpixel(x2 + 1, y2--, color);
    }
}

static __attribute__((noinline)) void oval64(int x1, int y1, int x2, int y2,
                                             uint8_t color)
{
    int64_t a = (int64_t)x2 - x1;
    int64_t b = (int64_t)y2 - y1;
    int64_t odd = b & 1;
    int64_t dx = 4 * (1 - a) * b * b;
    int64_t dy = 4 * (odd + 1) * a * a;
    int64_t error = dx + dy + odd * a * a;
    y1 += (b + 1) / 2;
    y2 = y1 - odd;
    a *= 8 * a;
    odd = 8 * b * b;

    do {
        vga_putpixel(x2, y1, color);
        vga_putpixel(x1, y1, color);
        vga_putpixel(x1, y2, color);
        vga_putpixel(x2, y2, color);
        int64_t twice = 2 * error;
        if (twice <= dy) {
            ++y1;
            --y2;
            dy += a;
            error += dy;
        }
        if (twice >= dx || 2 * error > dy) {
            ++x1;
            --x2;
            dx += odd;
            error += dx;
        }
    } while (x1 <= x2);

    while ((int64_t)y1 - y2 <= b) {
        vga_putpixel(x1 - 1, y1, color);
        vga_putpixel(x2 + 1, y1++, color);
        vga_putpixel(x1 - 1, y2, color);
        vga_putpixel(x2 + 1, y2--, color);
    }
}

void vga_oval(int x1, int y1, int x2, int y2, uint8_t color)
{
    order_bounds(&x1, &x2);
    order_bounds(&y1, &y2);
    if (x2 < 0 || y2 < 0 || x1 >= VGA_MODE_X_WIDTH || y1 >= VGA_MODE_X_HEIGHT)
        return;
    if (x1 == x2 || y1 == y2) {
        vga_fillrect(x1, y1, x2, y2, color);
        return;
    }
    if (bounds_visible(x1, y1, x2, y2)) {
        oval_unchecked(x1, y1, x2, y2, color);
        return;
    }

    /* The midpoint intermediates fit in 32 bits within these bounds. */
    if ((unsigned)x2 - (unsigned)x1 <= 384 &&
        (unsigned)y2 - (unsigned)y1 <= 384) {
        oval32(x1, y1, x2, y2, color);
    } else {
        oval64(x1, y1, x2, y2, color);
    }
}

static void roundrect_unchecked(int left, int top, int right, int bottom,
                                int radius, uint8_t color)
{
    int x = radius;
    int y = 0;
    int error = 1 - radius;
    int left_x = column_offset(left - x);
    int right_x = column_offset(right + x);
    int left_y = column_offset(left);
    int right_y = column_offset(right);
    int top_x = (top - x) * MODE_X_STRIDE;
    int bottom_x = (bottom + x) * MODE_X_STRIDE;
    int top_y = top * MODE_X_STRIDE;
    int bottom_y = bottom * MODE_X_STRIDE;
    while (x >= y) {
        backbuffer[left_x + top_y] = color;
        backbuffer[left_y + top_x] = color;
        backbuffer[right_x + top_y] = color;
        backbuffer[right_y + top_x] = color;
        backbuffer[left_x + bottom_y] = color;
        backbuffer[left_y + bottom_x] = color;
        backbuffer[right_x + bottom_y] = color;
        backbuffer[right_y + bottom_x] = color;
        ++y;
        left_y += -1;
        right_y += 1;
        top_y -= MODE_X_STRIDE;
        bottom_y += MODE_X_STRIDE;
        if (error < 0) {
            error += 2 * y + 1;
        } else {
            --x;
            left_x += 1;
            right_x += -1;
            top_x += MODE_X_STRIDE;
            bottom_x -= MODE_X_STRIDE;
            error += 2 * (y - x) + 1;
        }
    }
}

static void roundrect32(int left, int top, int right, int bottom, int radius,
                        uint8_t color)
{
    int x = radius;
    int y = 0;
    int error = 1 - radius;
    while (x >= y) {
        vga_putpixel(left - x, top - y, color);
        vga_putpixel(left - y, top - x, color);
        vga_putpixel(right + x, top - y, color);
        vga_putpixel(right + y, top - x, color);
        vga_putpixel(left - x, bottom + y, color);
        vga_putpixel(left - y, bottom + x, color);
        vga_putpixel(right + x, bottom + y, color);
        vga_putpixel(right + y, bottom + x, color);
        ++y;
        if (error < 0) {
            error += 2 * y + 1;
        } else {
            --x;
            error += 2 * (y - x) + 1;
        }
    }
}

static __attribute__((noinline)) void roundrect64(int left, int top, int right,
                                                  int bottom, int radius,
                                                  uint8_t color)
{
    int x = radius;
    int y = 0;
    int64_t error = 1 - radius;
    while (x >= y) {
        vga_putpixel(left - x, top - y, color);
        vga_putpixel(left - y, top - x, color);
        vga_putpixel(right + x, top - y, color);
        vga_putpixel(right + y, top - x, color);
        vga_putpixel(left - x, bottom + y, color);
        vga_putpixel(left - y, bottom + x, color);
        vga_putpixel(right + x, bottom + y, color);
        vga_putpixel(right + y, bottom + x, color);
        ++y;
        if (error < 0) {
            error += 2 * (int64_t)y + 1;
        } else {
            --x;
            error += 2 * ((int64_t)y - x) + 1;
        }
    }
}

void vga_roundrect(int x1, int y1, int x2, int y2, int radius, uint8_t color)
{
    order_bounds(&x1, &x2);
    order_bounds(&y1, &y2);
    if (x2 < 0 || y2 < 0 || x1 >= VGA_MODE_X_WIDTH || y1 >= VGA_MODE_X_HEIGHT)
        return;
    if (radius <= 0) {
        vga_rect(x1, y1, x2, y2, color);
        return;
    }
    unsigned max_radius = ((unsigned)x2 - (unsigned)x1) / 2;
    unsigned max_y_radius = ((unsigned)y2 - (unsigned)y1) / 2;
    if (max_y_radius < max_radius)
        max_radius = max_y_radius;
    if ((unsigned)radius > max_radius)
        radius = max_radius;
    if (!radius) {
        vga_rect(x1, y1, x2, y2, color);
        return;
    }

    int left = x1 + radius;
    int right = x2 - radius;
    int top = y1 + radius;
    int bottom = y2 - radius;
    if (bounds_visible(x1, y1, x2, y2)) {
        horizontal_span_unchecked(left, y1, right, color);
        horizontal_span_unchecked(left, y2, right, color);
        vertical_span_unchecked(x1, top, bottom, color);
        vertical_span_unchecked(x2, top, bottom, color);
        roundrect_unchecked(left, top, right, bottom, radius, color);
        return;
    }
    horizontal_span(left, y1, right, color);
    horizontal_span(left, y2, right, color);
    vertical_span(x1, top, bottom, color);
    vertical_span(x2, top, bottom, color);
    if (radius <= 32767) {
        roundrect32(left, top, right, bottom, radius, color);
    } else {
        roundrect64(left, top, right, bottom, radius, color);
    }
}

static bool clip_blit_axis(int* src, int src_limit, int* dst, int dst_limit,
                           int* count)
{
    int64_t first = 0;
    int64_t last = *count;
    if (*src < 0)
        first = -(int64_t)*src;
    if (-(int64_t)*dst > first)
        first = -(int64_t)*dst;
    if ((int64_t)src_limit - *src < last)
        last = (int64_t)src_limit - *src;
    if ((int64_t)dst_limit - *dst < last)
        last = (int64_t)dst_limit - *dst;
    if (first >= last)
        return false;
    *src += first;
    *dst += first;
    *count = last - first;
    return true;
}

void vga_blit(struct Bitmap* src, int x1, int y1, struct Bitmap* dst, int x2,
              int y2, int width, int height)
{
    if (!src)
        src = &screen;
    if (!dst)
        dst = &screen;
    if (!clip_blit_axis(&x1, src->width, &x2, dst->width, &width) ||
        !clip_blit_axis(&y1, src->height, &y2, dst->height, &height))
        return;
    bool overlap = src->pixels == dst->pixels;
    int step = overlap && y2 > y1 ? -1 : 1;
    int row = step < 0 ? height - 1 : 0;
    if (!overlap && width == src->width && width == dst->width) {
        memcpy(dst->pixels + (size_t)y2 * dst->width,
               src->pixels + (size_t)y1 * src->width, (size_t)width * height);
        return;
    }
    for (int n = 0; n < height; ++n, row += step)
        memmove(dst->pixels + (size_t)(y2 + row) * dst->width + x2,
                src->pixels + (size_t)(y1 + row) * src->width + x1, width);
}

uint64_t sys_perf_clock(void)
{
    return syscall64(SYSCALL_PERF_CLOCK, 0, 0, 0, 0, 0);
}

int sys_vga_swap_rect(int x1, int y1, int x2, int y2)
{
    return syscall(SYSCALL_VGA_SWAP_RECT, x1, y1, x2, y2, 0);
}
