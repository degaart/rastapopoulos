#ifndef FIRMWARE_SOUNDS_H
#define FIRMWARE_SOUNDS_H
#include <stdint.h>
typedef struct
{
    uint16_t frequency_hz;
    uint8_t ticks;
} FirmwareTone;
typedef struct
{
    unsigned id, count;
    const FirmwareTone* tones;
} FirmwareSound;
extern const FirmwareSound firmware_sounds[];
extern const unsigned firmware_sound_count;
#endif
