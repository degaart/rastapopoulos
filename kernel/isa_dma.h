#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum IsaDmaDirection
{
    ISA_DMA_FROM_DEVICE,
    ISA_DMA_TO_DEVICE,
};

bool isa_dma_claim(unsigned channel);
void isa_dma_release(unsigned channel);
bool isa_dma_start(unsigned channel, uintptr_t physical, size_t bytes,
                   enum IsaDmaDirection direction, bool auto_initialize);
void isa_dma_stop(unsigned channel);
size_t isa_dma_remaining(unsigned channel);
