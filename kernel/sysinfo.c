#include "sysinfo.h"
#include "kthread.h"
#include "pit.h"
#include "pmm.h"
#include "vga.h"
#include <stdio.h>

struct Event* sysinfo_event;

static void sysinfo_entry(void*)
{
    char buf[64];
    while (1) {
        event_wait(sysinfo_event);
        // int len = snprintf(buf, sizeof(buf), "%zu %llu", pmm_info(),
        //                    pit_get_ticks());

        int len = snprintf(buf, sizeof(buf), "%llu", pit_get_ticks());
        for (int i = 0; i < len; i++) {
            vga_write_at(VGA_WIDTH - len + i, 2, buf[i], VGA_COLOR_WHITE,
                         VGA_COLOR_BLUE);
        }
    }
}

void sysinfo_init(void)
{
    sysinfo_event = event_create();
    thread_create("sysinfo", sysinfo_entry, NULL);
}

