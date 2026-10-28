#include "isa_dma.h"
#include "kernel.h"

static const uint16_t pages[8] = {0x87, 0x83, 0x81, 0x82, 0, 0x8b, 0x89, 0x8a};
static bool claimed[8];
static bool running[8];
static bool automatic[8];
static uint8_t terminal[2];

static bool valid_channel(unsigned channel)
{
    return channel < 8 && channel != 4;
}

static uint16_t control_port(unsigned channel, unsigned byte_port)
{
    return channel < 4 ? byte_port : 0xc0 + byte_port * 2;
}

static void mask(unsigned channel, bool masked)
{
    outb(control_port(channel, 0x0a), (channel & 3) | (masked ? 4 : 0));
}

bool isa_dma_claim(unsigned channel)
{
    if (!valid_channel(channel))
        return false;
    bool flag = enter_critical_section();
    bool result = !claimed[channel];
    if (result) {
        claimed[channel] = true;
        mask(channel, true);
    }
    leave_critical_section(flag);
    return result;
}

void isa_dma_stop(unsigned channel)
{
    if (!valid_channel(channel))
        return;
    bool flag = enter_critical_section();
    if (claimed[channel]) {
        mask(channel, true);
        running[channel] = false;
    }
    leave_critical_section(flag);
}

void isa_dma_release(unsigned channel)
{
    if (!valid_channel(channel))
        return;
    bool flag = enter_critical_section();
    isa_dma_stop(channel);
    claimed[channel] = false;
    leave_critical_section(flag);
}

bool isa_dma_start(unsigned channel, uintptr_t physical, size_t bytes,
                   enum IsaDmaDirection direction, bool auto_initialize)
{
    if (!valid_channel(channel) || bytes == 0 || physical >= 0x1000000 ||
        bytes > 0x1000000 - physical ||
        (direction != ISA_DMA_FROM_DEVICE && direction != ISA_DMA_TO_DEVICE))
        return false;
    unsigned shift = channel < 4 ? 0 : 1;
    size_t boundary = 0x10000u << shift;
    if (bytes > boundary || (physical & (boundary - 1)) + bytes > boundary ||
        ((physical | bytes) & shift))
        return false;

    bool flag = enter_critical_section();
    if (!claimed[channel]) {
        leave_critical_section(flag);
        return false;
    }
    mask(channel, true);
    unsigned controller = channel >> 2;
    unsigned local = channel & 3;
    terminal[controller] |= inb(control_port(channel, 8)) & 15;
    terminal[controller] &= ~(1u << local);
    outb(control_port(channel, 0x0c), 0);
    uint16_t address_port = channel < 4 ? local * 2 : 0xc0 + local * 4;
    uint16_t count_port = address_port + (1u << shift);
    unsigned address = physical >> shift;
    unsigned count = (bytes >> shift) - 1;
    outb(address_port, address & 0xff);
    outb(address_port, (address >> 8) & 0xff);
    outb(pages[channel], physical >> 16);
    outb(control_port(channel, 0x0c), 0);
    outb(count_port, count & 0xff);
    outb(count_port, count >> 8);
    outb(control_port(channel, 0x0b),
         local | 0x40 | (direction == ISA_DMA_FROM_DEVICE ? 4 : 8) |
             (auto_initialize ? 0x10 : 0));
    automatic[channel] = auto_initialize;
    running[channel] = true;
    mask(channel, false);
    leave_critical_section(flag);
    return true;
}

size_t isa_dma_remaining(unsigned channel)
{
    if (!valid_channel(channel))
        return 0;
    bool flag = enter_critical_section();
    if (!claimed[channel]) {
        leave_critical_section(flag);
        return 0;
    }
    mask(channel, true);
    unsigned controller = channel >> 2;
    terminal[controller] |= inb(control_port(channel, 8)) & 15;
    outb(control_port(channel, 0x0c), 0);
    unsigned shift = channel < 4 ? 0 : 1;
    uint16_t port = channel < 4 ? channel * 2 + 1 : 0xc2 + (channel & 3) * 4;
    unsigned count = inb(port);
    count |= (unsigned)inb(port) << 8;
    size_t result = (count + 1) << shift;
    if (!automatic[channel] && (terminal[controller] & (1u << (channel & 3))))
        result = 0;
    if (result == 0)
        running[channel] = false;
    if (running[channel])
        mask(channel, false);
    leave_critical_section(flag);
    return result;
}
