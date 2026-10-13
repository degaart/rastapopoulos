#pragma once

#include <stdbool.h>
#include <stddef.h>

/* Fixed-capacity, value-copying FIFO. Callers provide synchronization
 * Zero initialization is sufficient. Failed push/pop leave all data unchanged
 * Types containing commas must be supplied through an existing type alias
 */
#define RING_BUFFER_DEFINE(Name, Type, Capacity)                              \
    _Static_assert((Capacity) > 0, "Ring buffer capacity must be positive");  \
    struct Name                                                               \
    {                                                                         \
        Type data[Capacity];                                                  \
        size_t read_index;                                                    \
        size_t write_index;                                                   \
        size_t count;                                                         \
    };                                                                        \
    static inline void Name##_init(struct Name* queue)                        \
    {                                                                         \
        queue->read_index = 0;                                                \
        queue->write_index = 0;                                               \
        queue->count = 0;                                                     \
    }                                                                         \
    static inline bool Name##_empty(struct Name const* queue)                 \
    {                                                                         \
        return queue->count == 0;                                             \
    }                                                                         \
    static inline bool Name##_full(struct Name const* queue)                  \
    {                                                                         \
        return queue->count == (Capacity);                                    \
    }                                                                         \
    static inline size_t Name##_count(struct Name const* queue)               \
    {                                                                         \
        return queue->count;                                                  \
    }                                                                         \
    static inline bool Name##_push(struct Name* queue, Type const* value)     \
    {                                                                         \
        if (Name##_full(queue))                                               \
            return false;                                                     \
        queue->data[queue->write_index] = *value;                             \
        if (++queue->write_index == (Capacity))                               \
            queue->write_index = 0;                                           \
        ++queue->count;                                                       \
        return true;                                                          \
    }                                                                         \
    static inline bool Name##_pop(struct Name* queue, Type* value)            \
    {                                                                         \
        if (Name##_empty(queue))                                              \
            return false;                                                     \
        *value = queue->data[queue->read_index];                              \
        if (++queue->read_index == (Capacity))                                \
            queue->read_index = 0;                                            \
        --queue->count;                                                       \
        return true;                                                          \
    }
