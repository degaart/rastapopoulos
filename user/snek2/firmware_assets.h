#ifndef FIRMWARE_ASSETS_H
#define FIRMWARE_ASSETS_H
#include <stdint.h>
typedef struct
{
    uint32_t descriptor_address, data_address;
    uint8_t width, height;
    const uint8_t*
        columns; /* x + (y / 8) * width, bit (y % 8), top bit is bit 0 */
} FirmwareBitmap;
extern const FirmwareBitmap firmware_food;
extern const FirmwareBitmap firmware_digits[10];
extern const FirmwareBitmap firmware_heads[4];
extern const FirmwareBitmap firmware_open_heads[4];
extern const FirmwareBitmap firmware_tails[4];
extern const FirmwareBitmap firmware_bodies[4];
extern const FirmwareBitmap firmware_swallow_bodies[4];
extern const FirmwareBitmap firmware_corners[4];
extern const FirmwareBitmap firmware_swallow_corners[4];
extern const FirmwareBitmap firmware_creatures[10];
#endif
