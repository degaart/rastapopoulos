#include "firmware_sounds.h"
static const FirmwareTone tones_31[] = {{2637, 1}}; /* ROM0x320264 */
static const FirmwareTone tones_32[] = {{440, 2}, {0, 2}, {440, 2}, {0, 2},
                                        {440, 2}, {0, 2}}; /* ROM0x32026C */
static const FirmwareTone tones_33[] = {
    {440, 1}, {0, 5}, {440, 1}, {0, 5}, {440, 1}, {0, 5},
    {440, 1}, {0, 5}, {440, 1}, {0, 5}, {440, 1}, {0, 5},
    {440, 1}, {0, 5}, {440, 1}, {0, 5}}; /* ROM0x320278 */
static const FirmwareTone tones_34[] = {
    {523, 15}, {0, 3}, {784, 56}}; /* ROM0x320284 */
static const FirmwareTone tones_35[] = {
    {587, 18}, {0, 18}, {587, 15}, {0, 3}, {587, 15}, {0, 3},
    {880, 52}, {0, 3},  {587, 15}, {0, 3}, {880, 56}}; /* ROM0x320290 */
const FirmwareSound firmware_sounds[] = {
    {31, 1, tones_31}, {32, 6, tones_32},  {33, 16, tones_33},
    {34, 3, tones_34}, {35, 11, tones_35},
};
const unsigned firmware_sound_count =
    sizeof(firmware_sounds) / sizeof(firmware_sounds[0]);
