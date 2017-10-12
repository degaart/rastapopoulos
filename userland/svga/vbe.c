#include "vbe.h"
#include <string.h>
#include <runtime.h>
#include <debug.h>

struct vbe_info* vbe_info()
{
    struct vbe_info* vbeinfo = (struct vbe_info*)LOWMEM_START;
    memset(vbeinfo, 0, sizeof(struct vbe_info));
    memcpy(vbeinfo->signature, "VBE2", 4);

    struct int10_regs regs = {
        .eax = 0x4F00,
        .es = 0,
        .edi = (uint32_t)vbeinfo
    };

    int10(&regs);

    if(regs.eax != 0x004F)
        return NULL;

    return vbeinfo;
}

struct vbe_modeinfo* vbe_modeinfo(int mode)
{
    struct vbe_modeinfo* result = (struct vbe_modeinfo*)LOWMEM_START;
    struct int10_regs regs = {
        .eax = 0x4F01,
        .ecx = mode,
        .es = 0,
        .edi = (uint32_t)result,
    };
    int10(&regs);

    if(regs.eax != 0x004f)
        return NULL;

    return result;
}

int vbe_setmode(int mode)
{
    static const int MODE_MASK = 0x3FFF;
    static const int MODE_LFB = 1 << 14;
    struct int10_regs regs = {
        .eax = 0x4F02,
        .ebx = (mode & MODE_MASK) | MODE_LFB
    };
    assert(regs.es == 0);
    assert(regs.edi == 0);
    int10(&regs);

    if(regs.eax != 0x004F)
        return 1;
    return 0;
}

int vbe_current_mode()
{
    struct int10_regs regs = {
        .eax = 0x4F03
    };
    int10(&regs);

    if(regs.eax != 0x4F00)
        return 1;

    return regs.ebx;
}

