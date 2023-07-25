#include "vga.h"
#include "debug.h"
#include "io.h"
#include "string.h"
#include "util.h"
#include <stdint.h>

static unsigned vga_cursor_x;
static unsigned vga_cursor_y;
bool vga_enable = true;

void vga_init() { vga_cursor_pos(&vga_cursor_x, &vga_cursor_y); }

void vga_cursor_pos(unsigned *x, unsigned *y) {
  outb(0x3D4, 0x0F);
  uint8_t pos_lo = inb(0x3D5);
  outb(0x3D4, 0x0E);
  uint8_t pos_hi = inb(0x3D5);
  uint16_t pos = pos_lo | (pos_hi << 8);

  *x = pos % VGA_WIDTH;
  *y = pos / VGA_WIDTH;
}

void vga_set_cursor_pos(unsigned x, unsigned y) {
  if (!vga_enable)
    return;

  assert(x < VGA_WIDTH);
  assert(y < VGA_HEIGHT);
  uint16_t pos = (y * VGA_WIDTH) + x;
  outb(0x3D4, 0x0F);
  outb(0x3D5, pos & 0xFF);
  outb(0x3D4, 0x0E);
  outb(0x3D5, (pos >> 8) & 0xFF);
}

/*
 * writes single character and attribute at specified position
 */
void vga_put_char(unsigned x, unsigned y, unsigned ch, unsigned attr) {
  if (!vga_enable)
    return;
  volatile uint16_t *ptr = ((uint16_t *)VGA_BASE + (y * VGA_WIDTH)) + x;
  *ptr = (ch & 0xFF) | ((attr & 0xFF) << 8);
}

/*
 * Write single char and attribute at current cursor pos and update cursor pos
 */
void vga_write_char(unsigned ch, unsigned attr) {
  if (!vga_enable)
    return;
  switch (ch) {
  case '\r':
    // ignore
    break;
  case '\n':
    vga_cursor_x = 0;
    if (vga_cursor_y < VGA_HEIGHT - 1) {
      vga_cursor_y++;
    } else {
      vga_scroll();
    }
    break;
  default:
    vga_put_char(vga_cursor_x, vga_cursor_y, ch, attr);
    if (vga_cursor_x < VGA_WIDTH - 1) {
      vga_cursor_x++;
    } else if (vga_cursor_y < VGA_HEIGHT - 1) {
      vga_cursor_x = 0;
      vga_cursor_y++;
    } else {
      vga_scroll();
      vga_cursor_x = 0;
    }
  }
  vga_set_cursor_pos(vga_cursor_x, vga_cursor_y);
}

void vga_write_string(const char *s, unsigned attr) {
  if (!vga_enable)
    return;
  CLEAR_IF();
  while (*s) {
    vga_write_char(*s, attr);
    s++;
  }
  RESTORE_IF();
}

void vga_scroll() {
  if (!vga_enable)
    return;

  memcpy((void *)VGA_BASE, (void *)(VGA_BASE + (VGA_WIDTH * 2)),
         VGA_WIDTH * (VGA_HEIGHT - 1) * 2);
  for (size_t i = 0; i < VGA_WIDTH; i++) {
    vga_put_char(i, VGA_HEIGHT - 1, ' ', COLOR_LIGHTGRAY);
  }
}
