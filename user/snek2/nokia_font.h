#ifndef NOKIA_FONT_H
#define NOKIA_FONT_H
#include <stdint.h>
typedef enum
{
    NOKIA_LARGE_BOLD,
    NOKIA_SMALL_PLAIN,
    NOKIA_SMALL_BOLD,
    NOKIA_TINY_PLAIN
} NokiaFont;
typedef struct
{
    uint8_t width, height;
    uint16_t rows[16];
} NokiaGlyph;
const NokiaGlyph* nokia_glyph(NokiaFont font, unsigned char ch);
int nokia_text_width(NokiaFont font, const char* text);
#endif
