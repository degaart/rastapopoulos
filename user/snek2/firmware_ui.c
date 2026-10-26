#include "firmware_ui.h"
#include "nokia_title.h"
const FirmwareBitmap firmware_title = {0x316884, 0x316214, 84, 48,
                                       nokia_title};
const FirmwareBitmap firmware_title_frames[5] = {
    {0x316890, 0x31640c, 48, 32, nokia_title_frame_1},
    {0x31689c, 0x3164cc, 48, 34, nokia_title_frame_2},
    {0x3168a8, 0x3165bc, 47, 35, nokia_title_frame_3},
    {0x3168b4, 0x3166a8, 48, 33, nokia_title_frame_4},
    {0x3168c0, 0x316798, 47, 33, nokia_title_frame_5},
};
/* English PPM string at physical 0x346b3c. */
const char firmware_snake_instructions[] =
    "Make the snake grow longer by directing it to the food. Use the keys 2, "
    "4, 6 and 8. You cannot stop the snake or make it go backwards. Try not "
    "to hit the walls or the tail.";
