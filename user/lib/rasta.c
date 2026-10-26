#include "rasta.h"
#include "../../kernel/syscall.h"

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

void sys_putpixel(unsigned x, unsigned y, uint8_t color)
{
    syscall(SYSCALL_PUTPIXEL, x, y, color, 0, 0);
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

int sys_vga_fillrect(int x1, int y1, int x2, int y2, uint8_t color)
{
    return syscall(SYSCALL_VGA_FILLRECT, x1, y1, x2, y2, color);
}

int sys_vga_rect(int x1, int y1, int x2, int y2, uint8_t color)
{
    return syscall(SYSCALL_VGA_RECT, x1, y1, x2, y2, color);
}

int sys_vga_drawline(int x1, int y1, int x2, int y2, uint8_t color)
{
    return syscall(SYSCALL_VGA_DRAWLINE, x1, y1, x2, y2, color);
}

int sys_vga_oval(int x1, int y1, int x2, int y2, uint8_t color)
{
    return syscall(SYSCALL_VGA_OVAL, x1, y1, x2, y2, color);
}

int sys_vga_roundrect(int x1, int y1, int x2, int y2, int radius,
                      uint8_t color)
{
    struct SyscallVGARoundrect args = {x1, y1, x2, y2, radius, color};
    return syscall(SYSCALL_VGA_ROUNDRECT, (uintptr_t)&args, 0, 0, 0, 0);
}

int sys_vga_blit(VGABitmap src, int x1, int y1, VGABitmap dst, int x2, int y2,
                 int width, int height)
{
    struct SyscallVGABlit args = {src, x1, y1, dst, x2, y2, width, height};
    return syscall(SYSCALL_VGA_BLIT, (uintptr_t)&args, 0, 0, 0, 0);
}

VGABitmap sys_vga_load_bitmap(const void* buffer, size_t len, int width,
                              int height)
{
    return syscall(SYSCALL_VGA_LOAD_BITMAP, (uintptr_t)buffer, len, width,
                   height, 0);
}

int sys_vga_destroy_bitmap(VGABitmap bitmap)
{
    return syscall(SYSCALL_VGA_DESTROY_BITMAP, bitmap, 0, 0, 0, 0);
}

int sys_vga_set_palette(uint8_t index, uint8_t red, uint8_t green,
                        uint8_t blue)
{
    return syscall(SYSCALL_VGA_SET_PALETTE, index, red, green, blue, 0);
}

