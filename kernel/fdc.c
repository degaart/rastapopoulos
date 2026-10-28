#include "fdc.h"
#include "address_space.h"
#include "cmos.h"
#include "isa_dma.h"
#include "kmalloc.h"
#include "kthread.h"
#include "pic.h"
#include "pit.h"
#include "pmm.h"
#include "process.h"
#include "trace.h"
#include "vmm.h"
#include <assert.h>
#include <queue.h>
#include <string.h>

#define FDC_DOR   0x3f2
#define FDC_MSR   0x3f4
#define FDC_FIFO  0x3f5
#define FDC_DIR   0x3f7
#define READY_MS  500
#define IRQ_MS    2000
#define SPIN_MS   500
#define SETTLE_MS 20

enum FdcMode
{
    FDC_MODE_AT,
    FDC_MODE_PS2,
    FDC_MODE_MODEL30,
};

struct FdcGeometry
{
    unsigned cylinders;
    unsigned sectors;
    uint8_t rate;
    uint8_t gap;
};

struct FdcChs
{
    unsigned cylinder;
    unsigned head;
    unsigned sector;
};

struct Request
{
    volatile int refcount;
    struct Event done;
    unsigned drive;
    unsigned lba;
    size_t length;
    size_t completed;
    bool write;
    bool canceled;
    bool success;
    TAILQ_ENTRY(Request) queue;
    char data[];
};
TAILQ_HEAD(RequestList, Request);

static struct Event request_event;
static struct Event irq_event;
static struct Event init_event;
static struct RequestList queue = TAILQ_HEAD_INITIALIZER(queue);
static unsigned drive_types[2];
static int cylinders[2] = {-1, -1};
static int heads[2] = {-1, -1};
static enum FdcMode mode;
static uint8_t dor = 0x0c;
static uint8_t data_rate;
static bool started;
static bool ready;
static bool controller_ok;
static char* bounce;
static uintptr_t bounce_physical;

#ifdef UNIT_TESTS
static unsigned attempt_count;

unsigned fdc_test_attempt_count(void)
{
    return attempt_count;
}
#endif

static struct Request* request_create(size_t length)
{
    struct Request* request = kmalloc(sizeof(struct Request) + length);
    if (!request)
        return NULL;
    memset(request, 0, sizeof(*request));
    request->refcount = 1;
    return request;
}

static int request_incref(struct Request* request)
{
    bool iflag = enter_critical_section();
    int result = ++request->refcount;
    leave_critical_section(iflag);
    return result;
}

static int request_decref(struct Request* request)
{
    bool iflag = enter_critical_section();
    int result = --request->refcount;
    if (result) {
        int result = request->refcount;
        leave_critical_section(iflag);
        return result;
    }
    leave_critical_section(iflag);
    kfree(request);
    return result;
}

static void irq6_handler(int irq, struct isr_regs* regs)
{
    event_signal(&irq_event);
    pic_eoi(irq);
}

static bool fifo_ready(bool input)
{
    uint64_t start = pit_get_ticks();
    do {
        if ((inb(FDC_MSR) & 0xc0) == (input ? 0xc0 : 0x80))
            return true;
        thread_sleep_ms(1);
    } while (pit_get_ticks() - start < pit_ms_to_ticks(READY_MS));
    return false;
}

static bool send_byte(uint8_t value)
{
    if (!fifo_ready(false))
        return false;
    outb(FDC_FIFO, value);
    return true;
}

static bool receive_byte(uint8_t* value)
{
    if (!fifo_ready(true))
        return false;
    *value = inb(FDC_FIFO);
    return true;
}

static bool command(const uint8_t* bytes, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (!send_byte(bytes[i]))
            return false;
    }
    return true;
}

static void clear_irq(void)
{
    event_wait(&irq_event, 0);
}

static bool sense(uint8_t* status, uint8_t* cylinder)
{
    return send_byte(8) && receive_byte(status) && *status != 0x80 &&
           receive_byte(cylinder);
}

static bool specify(void)
{
    uint8_t bytes[] = {3, 0xdf, 0x02};
    return command(bytes, sizeof(bytes));
}

static bool reset(void)
{
    isa_dma_stop(2);
    cylinders[0] = cylinders[1] = -1;
    heads[0] = heads[1] = -1;
    outb(FDC_DOR, 0);
    dor = 0x0c;
    thread_sleep_ms(2);
    clear_irq();
    outb(FDC_DOR, dor);
    if (!event_wait(&irq_event, IRQ_MS))
        return false;
    for (unsigned i = 0; i < 4; i++) {
        uint8_t status, cylinder;
        if (!sense(&status, &cylinder) || (status & 0xc3) != (0xc0 | i))
            return false;
    }
    outb(FDC_DIR, data_rate);
    uint8_t version;
    if (!send_byte(0x10) || !receive_byte(&version))
        return false;
    if (version == 0x90) {
        uint8_t configure[] = {0x13, 0, 0x17, 0};
        if (!command(configure, sizeof(configure)))
            return false;
    }
    return specify();
}

static void motor(unsigned drive)
{
    uint8_t bit = 0x10u << drive;
    bool spin = !(dor & bit);
    dor = (dor & ~3u) | drive | bit;
    outb(FDC_DOR, dor);
    if (spin)
        thread_sleep_ms(SPIN_MS);
}

static bool position(unsigned drive, unsigned head, unsigned cylinder,
                     bool recalibrate)
{
    clear_irq();
    uint8_t bytes[] = {recalibrate ? 7 : 15, (head << 2) | drive, cylinder};
    if (!command(bytes, recalibrate ? 2 : 3) ||
        !event_wait(&irq_event, IRQ_MS))
        return false;
    uint8_t status, actual;
    if (!sense(&status, &actual) || (status & 0xfb) != (0x20 | drive) ||
        actual != cylinder)
        return false;
    cylinders[drive] = cylinder;
    heads[drive] = head;
    thread_sleep_ms(SETTLE_MS);
    return true;
}

static enum FdcMode fdc_probe_mode(uint8_t saved_dor, uint8_t saved_rate)
{
    bool ps2 = true;
    bool model30 = true;
    for (unsigned pass = 0; pass < 2; pass++) {
        for (unsigned rate = 0; rate < 3; rate += 2) {
            for (unsigned gate = 0; gate < 2; gate++) {
                outb(0x3f7, rate);
                outb(0x3f2, (saved_dor & ~8u) | (gate << 3));
                io_wait();
                uint8_t dir = inb(0x3f7) & 0x7f;
                ps2 &= dir == (0x78u | (rate == 2 ? 4u : 0u) | rate);
                model30 &= dir == ((gate << 3) | rate);
            }
        }
    }
    outb(0x3f7, saved_rate);
    outb(0x3f2, saved_dor);
    return ps2 ? FDC_MODE_PS2 : model30 ? FDC_MODE_MODEL30 : FDC_MODE_AT;
}

static bool fdc_changed(enum FdcMode mode, uint8_t dir)
{
    return ((dir & 0x80) != 0) != (mode == FDC_MODE_MODEL30);
}

static inline const struct FdcGeometry* fdc_geometry(unsigned type)
{
    static const struct FdcGeometry geometries[] = {
        [FDC_360] = {40, 9, 2, 0x2a},
        [FDC_1200] = {80, 15, 0, 0x1b},
        [FDC_720] = {80, 9, 2, 0x2a},
        [FDC_1440] = {80, 18, 0, 0x1b},
    };
    return type >= FDC_360 && type <= FDC_1440 ? &geometries[type] : NULL;
}

static inline struct FdcChs fdc_chs(const struct FdcGeometry* geometry,
                                    unsigned lba)
{
    return (struct FdcChs) {lba / (geometry->sectors * 2),
                            lba / geometry->sectors % 2,
                            lba % geometry->sectors + 1};
}

static bool sector(struct Request* request, unsigned lba)
{
    unsigned drive = request->drive;
    const struct FdcGeometry* geometry = fdc_geometry(drive_types[drive]);
    struct FdcChs chs = fdc_chs(geometry, lba);
    unsigned cylinder = chs.cylinder;
    unsigned head = chs.head;
    unsigned number = chs.sector;
    for (unsigned attempt = 0; attempt < 4; attempt++) {
#ifdef UNIT_TESTS
        attempt_count++;
#endif
        if (request->canceled)
            return false;
        if (!controller_ok) {
            controller_ok = reset();
            if (!controller_ok)
                continue;
        }
        data_rate = geometry->rate;
        outb(FDC_DIR, data_rate);
        motor(drive);
        if (!specify())
            goto recover;
        if (fdc_changed(mode, inb(FDC_DIR))) {
            cylinders[drive] = -1;
            if (!position(drive, 0, 1, false))
                goto recover;
            cylinders[drive] = -1;
        }
        if (cylinders[drive] < 0 && !position(drive, 0, 0, true))
            goto recover;
        if ((cylinders[drive] != cylinder || heads[drive] != head) &&
            !position(drive, head, cylinder, false))
            goto recover;
        if (request->write)
            memcpy(bounce, request->data + request->completed, 512);
        clear_irq();
        if (!isa_dma_start(2, bounce_physical, 512,
                           request->write ? ISA_DMA_TO_DEVICE
                                          : ISA_DMA_FROM_DEVICE,
                           false))
            return false;
        uint8_t bytes[] = {request->write ? 0x45 : 0x46,
                           (head << 2) | drive,
                           cylinder,
                           head,
                           number,
                           2,
                           geometry->sectors,
                           geometry->gap,
                           0xff};
        if (!command(bytes, sizeof(bytes)) || !event_wait(&irq_event, IRQ_MS))
            goto recover;
        isa_dma_stop(2);
        uint8_t result[7];
        for (unsigned i = 0; i < sizeof(result); i++) {
            if (!receive_byte(&result[i]))
                goto recover;
        }
        if ((result[0] & ~0x20u) != ((head << 2) | drive) || result[1] != 0 ||
            result[2] != 0 || result[6] != 2 || isa_dma_remaining(2) != 0) {
            if (attempt == 3)
                trace("FDC result %x %x %x CHRN %u/%u/%u/%u DMA %zu",
                      result[0], result[1], result[2], result[3], result[4],
                      result[5], result[6], isa_dma_remaining(2));
            goto recover;
        }
        /* Terminal count may report the next sector's CHRN */
        unsigned next_cylinder = cylinder;
        unsigned next_number = number + 1;
        if (number == geometry->sectors) {
            next_cylinder++;
            next_number = 1;
        }
        if (!((result[3] == cylinder && result[4] == head &&
               result[5] == number) ||
              (result[3] == next_cylinder && result[4] == head &&
               result[5] == next_number)))
            goto recover;
        if (!request->write)
            memcpy(request->data + request->completed, bounce, 512);
        return true;
    recover:
        isa_dma_stop(2);
        controller_ok = false;
        cylinders[0] = cylinders[1] = -1;
        heads[0] = heads[1] = -1;
    }
    trace("FDC drive %u LBA %u %s failed after 4 attempts", drive, lba,
          request->write ? "write" : "read");
    return false;
}

static void free_bounce(void)
{
    if (!vmm_unmap(bounce))
        panic("Failed to unmap FDC DMA buffer");
    bounce = NULL;
    bounce_physical = 0;
}

static void worker(void* unused)
{
    outb(FDC_DOR, dor);
    outb(FDC_DIR, data_rate);
    mode = fdc_probe_mode(dor, data_rate);
    for (unsigned attempt = 0; attempt < 4 && !controller_ok; attempt++)
        controller_ok = reset();
    ready = controller_ok;
    trace("FDC mode: %s, initialization: %s",
          mode == FDC_MODE_PS2       ? "PS/2"
          : mode == FDC_MODE_MODEL30 ? "Model 30"
                                     : "PC-AT",
          ready ? "ready" : "failed");
    if (!ready) {
        pic_set_irq_handler(IRQ_FDC, NULL);
        isa_dma_release(2);
        free_bounce();
    }
    event_signal(&init_event);
    if (!ready)
        return;
    while (true) {
        bool flag = enter_critical_section();
        struct Request* request = TAILQ_FIRST(&queue);
        if (!request) {
            dor &= 0x0f;
            outb(FDC_DOR, dor);
            event_wait(&request_event, WAIT_INFINITE);
            leave_critical_section(flag);
            continue;
        }
        TAILQ_REMOVE(&queue, request, queue);
        leave_critical_section(flag);

        while (request->completed < request->length &&
               sector(request, request->lba + request->completed / 512))
            request->completed += 512;
        flag = enter_critical_section();
        request->success = request->completed == request->length;
        event_signal(&request->done);
        request_decref(request);
        leave_critical_section(flag);
    }
}

bool fdc_init(void)
{
    assert((read_eflags() & EFLAGS_IF) != 0);
    if (started)
        return ready;
    started = true;
    uint8_t types = cmos_read(CMOS_FLOPPY_DRIVES);
    drive_types[0] = types >> 4;
    drive_types[1] = types & 15;
    void* physical = pmm_alloc_below(0x1000000);
    if (!physical)
        return false;
    bounce = vmm_map_kernel(physical, VMM_WRITABLE);
    if (!bounce) {
        pmm_free(physical);
        return false;
    }
    bounce_physical = (uintptr_t)physical;
    if (!isa_dma_claim(2)) {
        free_bounce();
        return false;
    }
    pic_set_irq_handler(IRQ_FDC, irq6_handler);
    if (!thread_create(NULL, "fdc", worker, NULL)) {
        pic_set_irq_handler(IRQ_FDC, NULL);
        isa_dma_release(2);
        free_bounce();
        return false;
    }
    return event_wait(&init_event, WAIT_INFINITE) && ready;
}

static bool valid_buffer(const void* buffer, size_t length, bool write)
{
    uintptr_t start = (uintptr_t)buffer;
    if (start < KERNEL_OFFSET || length > UINTPTR_MAX - start)
        return false;
    for (uintptr_t page = ALIGN_DOWN(start, PAGE_SIZE);
         page <= ALIGN_DOWN(start + length - 1, PAGE_SIZE);
         page += PAGE_SIZE) {
        unsigned flags;
        if (!address_space_query(address_space_current(), (void*)page, NULL,
                                 &flags) ||
            (flags & PAGE_USER) || (write && !(flags & PAGE_WRITABLE)))
            return false;
    }
    return true;
}

static bool transfer(int drive, unsigned lba, void* buffer, size_t length,
                     bool write)
{
    if (!ready || drive < 0 || drive > 1 || length == 0 || length % 512 ||
        drive_types[drive] < FDC_360 || drive_types[drive] > FDC_1440)
        return false;
    const struct FdcGeometry* geometry = fdc_geometry(drive_types[drive]);
    unsigned capacity = geometry->cylinders * 2 * geometry->sectors;
    if (lba >= capacity || length / 512 > capacity - lba ||
        !valid_buffer(buffer, length, !write))
        return false;

    struct Request* request = request_create(length);
    if (!request)
        return false;
    request->drive = drive;
    request->lba = lba;
    request->length = length;
    request->write = write;
    if (write)
        memcpy(request->data, buffer, length);
    request_incref(request);

    bool flag = enter_critical_section();
    TAILQ_INSERT_TAIL(&queue, request, queue);
    event_signal(&request_event);
    bool completed = event_wait(&request->done, WAIT_INFINITE);
    bool result = completed && request->success;
    if (!completed)
        request->canceled = true;
    else if (!write)
        memcpy(buffer, request->data, request->completed);
    request_decref(request);
    leave_critical_section(flag);
    return result;
}

bool fdc_read(int drive, unsigned lba, char* buffer, size_t buffer_len)
{
    assert((read_eflags() & EFLAGS_IF) != 0);
    return transfer(drive, lba, buffer, buffer_len, false);
}

bool fdc_write(int drive, unsigned lba, const char* buffer, size_t buffer_len)
{
    assert((read_eflags() & EFLAGS_IF) != 0);
    return transfer(drive, lba, (void*)buffer, buffer_len, true);
}
