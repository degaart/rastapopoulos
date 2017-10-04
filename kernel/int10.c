#include "kdebug.h"
#include "string.h"
#include "int10_stub.h"
#include "io.h"

struct int10_regs {
    uint32_t eax, ebx, ecx, edx;
    uint32_t ebp, esi, edi;
    uint32_t es, fs, gs;
    uint32_t reserved;
} __attribute((packed));
void int10(struct int10_regs*);
struct coord {
    int row, col;
};
static struct coord get_cursor_pos()
{
    struct int10_regs regs = { .eax = 0x0300 };
    int10(&regs);

    struct coord result;
    result.row = (regs.edx & 0xFF00) >> 8;
    result.col = regs.edx & 0xFF;
    return result;
}

static void set_cursor_pos(int x, int y)
{
    /*
     * AH = 02h
     * BH = page number
     * 0-3 in modes 2&3
     * 0-7 in modes 0&1
     * 0 in graphics modes
     * DH = row (00h is top)
     * DL = column (00h is left)
     */
    struct int10_regs regs = {
        .eax = 0x0200,
        .ebx = 0,
        .edx = ((y & 0xFF) << 8) | (y & 0xFF)
    };
    int10(&regs);
}

static void write_char(int ch, int row, int col)
{
    /*
        AH = 0Ah
        AL = character to display
        BH = page number (00h to number of pages - 1) (see #00010)
        background color in 256-color graphics modes (ET4000)
        BL = attribute (PCjr, Tandy 1000 only) or color (graphics mode)
        if bit 7 set in <256-color graphics mode, character is XOR'ed
        onto screen
        CX = number of times to write character

        Return:
        Nothing
     */
    struct int10_regs regs;
    regs.eax = 0x0E | (ch & 0xFF);
    regs.ebx = 0x0007;
    int10(&regs);
}

static void write_string(const char* str, int attribute, int row, int col)
{
    /*
        VIDEO - WRITE STRING (AT and later,EGA)

        AH = 13h
        AL = write mode

        bit 0:
        Update cursor after writing

        bit 1:
        String contains alternating characters and attributes

        bits 2-7:
        Reserved (0).
        BH = page number.
        BL = attribute if string contains only characters.
        CX = number of characters in string.
        DH,DL = row,column at which to start writing.
        ES:BP -> string to write
     */
    size_t len = strlen(str) & 0xFF; /* 256 chars limit */
    struct int10_regs regs = {
        .eax = 0x1301,
        .ebx = attribute & 0xFF,
        .ecx = len,
        .edx = ((row & 0xFF) << 8) | (col & 0xFF),
        .ebp = 0x7b00,
        .es = 0,
    };
    memcpy((void*)0x7b00, str, len);
    int10(&regs);
}

#define VGA_80x25           0x03
#define VGA_320x200x8       0x13

static void set_video_mode(int mode)
{
    struct int10_regs regs = {
        .eax = (mode & 0xFF)
    };
    int10(&regs);
}

#define VGA_BASE 0xA0000
static void putpixel(int x, int y, int col)
{
    unsigned char* ptr = (unsigned char*)VGA_BASE;
    int offset = (320 * y) + x;
    if(offset >= 0 && offset < 320*200)
        ptr[offset] = col & 0xFF;
}

void test_int10()
{
    trace("Testing int10 calls");

    /* Copy stub to 0x7C00 */
    memcpy((void*)0x7C00, obj_int10_stub_bin, sizeof(obj_int10_stub_bin));

    /* Set video mode (320x200x8) */
    set_video_mode(VGA_320x200x8);

    int x = 0, y = 0, col = 0;
    while(y < 200) {
        putpixel(x, y, col);
        col++;
        if(col >= 256)
            col = 0;
        x++;
        if(x >= 320) {
            x = 0;
            y++;
        }
    }
}


