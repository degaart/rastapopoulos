#include "syscall.h"
#include "bitset.h"
#include "event_queue.h"
#include "exec.h"
#include "idt.h"
#include "kernel.h"
#include "kmalloc.h"
#include "kthread.h"
#include "pit.h"
#include "process.h"
#include "ps2kbd.h"
#include "ramdisk.h"
#include "scancode.h"
#include "tar.h"
#include "trace.h"
#include "vga.h"
#include "vmm.h"
#include <string.h>

static int32_t syscall_getticks(uint32_t* edx)
{
    uint64_t ticks = pit_get_ticks();
    *edx = ticks >> 32;
    return ticks & 0xffffffff;
}

static int32_t syscall_sleep(uint32_t ticks)
{
    thread_sleep(ticks);
    return -1;
}

static void syscall_exit(uint8_t status)
{
    process_exit(status);
}

static void syscall_exit_thread(void)
{
    thread_exit();
}

static int32_t syscall_get_kbd_event(struct KeyEvent* evt, int* overflow,
                                     int block)
{
    if (!vmm_is_valid_userptr(evt, sizeof(*evt), true) ||
        !vmm_is_valid_userptr(overflow, sizeof(*overflow), true)) {
        return -1;
    }

    bool result;
    bool foverflow;
    if (block)
        result = process_get_input_event(evt, &foverflow);
    else
        result = process_peek_input_event(evt, &foverflow);
    *overflow = foverflow;
    return result ? 0 : -1;
}

static int32_t syscall_write(const char* s, size_t len)
{
    if (len > INT32_MAX)
        return -1;
    if (!vmm_is_valid_userptr(s, len, false))
        return -1;
    vga_write(s, len, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    return len;
}

static int32_t syscall_exec(const char* filename, const char* args)
{
    if (!filename)
        return -1;
    if (!vmm_is_valid_userstr(filename))
        return -1;
    if (args && !vmm_is_valid_userstr(args))
        return -1;

    return exec(filename, args);
}

static int32_t syscall_shutdown(void)
{
    debugexit(1);
    outb(0xf4, 1);
    return 0;
}

struct ListContext
{
    void* buffer;
    size_t capacity;
    size_t len;
};

static void list_callback(const struct TarEntry* entry, void* param)
{
    struct ListContext* ctx = param;
    if (entry->is_dir)
        return;

    size_t name_len = strlen(entry->name);
    if (name_len == 0)
        return;
    if (ctx->capacity - ctx->len < name_len + 2)
        return;
    memcpy(ctx->buffer + ctx->len, entry->name, name_len + 1);
    ctx->len += name_len + 1;
}

static int32_t syscall_list(char* buffer, size_t len)
{
    if (!vmm_is_valid_userptr(buffer, len, true))
        return -1;

    struct Tar ramdisk;
    if (!tar_open(&ramdisk, ramdisk_start, ramdisk_end - ramdisk_start))
        return -1;

    struct ListContext ctx = {
        .buffer = buffer,
        .capacity = len,
        .len = 0,
    };
    if (!tar_list(&ramdisk, list_callback, &ctx))
        return -1;

    buffer[ctx.len] = '\0';
    return ctx.len;
}

static int32_t syscall_lock_screen(void)
{
    return vga_lock_screen();
}

static int32_t syscall_unlock_screen(void)
{
    return vga_unlock_screen();
}

static void syscall_putpixel(unsigned x, unsigned y, unsigned color)
{
    vga_putpixel(x, y, color);
}

static int32_t syscall_trace(const char* file, int file_len, int line,
                             const char* s, int len)
{
    if (!vmm_is_valid_userptr(file, file_len, false) ||
        !vmm_is_valid_userptr(s, len, false)) {
        return -1;
    }

    serial_printf("[%s:%.*s:%d] %.*s\n", process_current()->name, file_len,
                  file, line, len, s);
    return 0;
}

static void syscall_vga_swap(void)
{
    vga_swap();
}

struct UserBitmap
{
    struct UserBitmap* next;
    struct Process* owner;
    struct Bitmap* bitmap;
    unsigned handle;
};

static struct UserBitmap* user_bitmaps;
static unsigned next_bitmap_handle = 1;

static struct UserBitmap* find_bitmap(unsigned handle)
{
    for (struct UserBitmap* entry = user_bitmaps; entry; entry = entry->next) {
        if (entry->handle == handle && entry->owner == process_current())
            return entry;
    }
    return NULL;
}

void syscall_cleanup_bitmaps(struct Process* process)
{
    bool iflag = enter_critical_section();
    struct UserBitmap** link = &user_bitmaps;
    while (*link) {
        struct UserBitmap* entry = *link;
        if (entry->owner != process) {
            link = &entry->next;
            continue;
        }
        *link = entry->next;
        vga_destroy_bitmap(entry->bitmap);
        kfree(entry);
    }
    leave_critical_section(iflag);
}

static unsigned syscall_vga_load_bitmap(const void* buffer, size_t len,
                                        int width, int height)
{
    if (!next_bitmap_handle || width <= 0 || height <= 0 ||
        (size_t)width > len / (size_t)height)
        return 0;
    size_t size = (size_t)width * height;
    if (!vmm_is_valid_userptr(buffer, size, false))
        return 0;
    struct UserBitmap* entry = kmalloc(sizeof(*entry));
    if (!entry)
        return 0;
    struct Bitmap* bitmap = vga_load_bitmap(buffer, size, width, height);
    if (!bitmap) {
        kfree(entry);
        return 0;
    }
    *entry = (struct UserBitmap) {user_bitmaps, process_current(), bitmap,
                                  next_bitmap_handle++};
    user_bitmaps = entry;
    return entry->handle;
}

static int syscall_vga_destroy_bitmap(unsigned handle)
{
    struct UserBitmap** link = &user_bitmaps;
    while (*link) {
        struct UserBitmap* entry = *link;
        if (entry->handle == handle && entry->owner == process_current()) {
            *link = entry->next;
            vga_destroy_bitmap(entry->bitmap);
            kfree(entry);
            return 0;
        }
        link = &entry->next;
    }
    return -1;
}

static int syscall_vga_roundrect(const struct SyscallVGARoundrect* args)
{
    if (!vmm_is_valid_userptr(args, sizeof(*args), false))
        return -1;
    struct SyscallVGARoundrect a = *args;
    vga_roundrect(a.x1, a.y1, a.x2, a.y2, a.radius, a.color);
    return 0;
}

static int syscall_vga_blit(const struct SyscallVGABlit* args)
{
    if (!vmm_is_valid_userptr(args, sizeof(*args), false))
        return -1;
    struct SyscallVGABlit a = *args;
    struct UserBitmap* src = find_bitmap(a.src);
    struct UserBitmap* dst = find_bitmap(a.dst);
    if ((a.src && !src) || (a.dst && !dst))
        return -1;
    vga_blit(src ? src->bitmap : NULL, a.x1, a.y1, dst ? dst->bitmap : NULL,
             a.x2, a.y2, a.width, a.height);
    return 0;
}

/*
 * Syscall ABI:
 *  eax: function
 *  ebx: 1st arg
 *  ecx: 2nd arg
 *  edx: 3rd arg
 *  esi: 4th arg
 *  edi: 5th arg
 * Result in eax. Negative values denote errors
 * 64-bit result in eax:edx
 */
void syscall_handler(int, struct isr_regs* regs)
{
    if (thread_current()->process->exit_requested)
        thread_exit();

    switch (regs->eax) {
    case SYSCALL_GETTICKS:
    {
        uint32_t hi;
        regs->eax = syscall_getticks(&hi);
        regs->edx = hi;
        break;
    }
    case SYSCALL_SLEEP:
        regs->eax = syscall_sleep(regs->ebx);
        break;
    case SYSCALL_EXIT:
        syscall_exit(regs->ebx & 0xff);
        panic("Unreachable code");
    case SYSCALL_EXIT_THREAD:
        syscall_exit_thread();
        panic("Unreachable code");
    case SYSCALL_GET_KBD_EVENT:
        regs->eax = syscall_get_kbd_event((struct KeyEvent*)regs->ebx,
                                          (int*)regs->ecx, regs->edx);
        break;
    case SYSCALL_WRITE:
        regs->eax = syscall_write((const char*)regs->ebx, regs->ecx);
        break;
    case SYSCALL_EXEC:
        regs->eax =
            syscall_exec((const char*)regs->ebx, (const char*)regs->ecx);
        break;
    case SYSCALL_SHUTDOWN:
        regs->eax = syscall_shutdown();
        break;
    case SYSCALL_LIST:
        regs->eax = syscall_list((char*)regs->ebx, (size_t)regs->ecx);
        break;
    case SYSCALL_LOCK_SCREEN:
        regs->eax = syscall_lock_screen();
        break;
    case SYSCALL_UNLOCK_SCREEN:
        regs->eax = syscall_unlock_screen();
        break;
    case SYSCALL_PUTPIXEL:
        syscall_putpixel(regs->ebx, regs->ecx, regs->edx);
        regs->eax = 0;
        break;
    case SYSCALL_TRACE:
        regs->eax = syscall_trace((const char*)regs->ebx, regs->ecx, regs->edx,
                                  (const char*)regs->esi, regs->edi);
        break;
    case SYSCALL_VGA_SWAP:
        syscall_vga_swap();
        regs->eax = 0;
        break;
    case SYSCALL_VGA_FILLRECT:
        vga_fillrect((int)regs->ebx, (int)regs->ecx, (int)regs->edx,
                     (int)regs->esi, regs->edi);
        regs->eax = 0;
        break;
    case SYSCALL_VGA_RECT:
        vga_rect((int)regs->ebx, (int)regs->ecx, (int)regs->edx,
                 (int)regs->esi, regs->edi);
        regs->eax = 0;
        break;
    case SYSCALL_VGA_DRAWLINE:
        vga_drawline((int)regs->ebx, (int)regs->ecx, (int)regs->edx,
                     (int)regs->esi, regs->edi);
        regs->eax = 0;
        break;
    case SYSCALL_VGA_OVAL:
        vga_oval((int)regs->ebx, (int)regs->ecx, (int)regs->edx,
                 (int)regs->esi, regs->edi);
        regs->eax = 0;
        break;
    case SYSCALL_VGA_ROUNDRECT:
        regs->eax = syscall_vga_roundrect(
            (const struct SyscallVGARoundrect*)regs->ebx);
        break;
    case SYSCALL_VGA_BLIT:
        regs->eax = syscall_vga_blit((const struct SyscallVGABlit*)regs->ebx);
        break;
    case SYSCALL_VGA_LOAD_BITMAP:
        regs->eax = syscall_vga_load_bitmap((const void*)regs->ebx, regs->ecx,
                                            regs->edx, regs->esi);
        break;
    case SYSCALL_VGA_DESTROY_BITMAP:
        regs->eax = syscall_vga_destroy_bitmap(regs->ebx);
        break;
    default:
        regs->eax = (uint32_t)-1;
        break;
    }

    if (thread_current()->process->exit_requested)
        thread_exit();
}

void syscall_init(void)
{
    idt_set_handler(0x80, syscall_handler, 3);

    size_t bitset_count = 65536;
    size_t bitset_len = bitset_storage_size(bitset_count);
    uint32_t* bitset_storage = kmalloc(bitset_len);
    if (!bitset_storage)
        panic("Out of memory");
}

