#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RBUF_DECLARE(structname, elemtype)                                     \
    struct structname {                                                        \
        elemtype* buf;                                                         \
        size_t head, tail, nelem;                                              \
    };                                                                         \
    typedef struct structname structname##_t

/*
 * Initialize a ring buffer with a buffer with elem_count storage size
 * The maximum number of elements which can be stored in the ring buffer
 * will be elem_count - 1
 */
#define RBUF_INIT(rbuf, buffer, elem_count)                                    \
    do {                                                                       \
        (rbuf)->buf = buffer;                                                  \
        (rbuf)->head = (rbuf)->tail = 0;                                       \
        (rbuf)->nelem = elem_count;                                            \
    } while(0)

#define RBUF_PUSH(rbuf, val)                                                   \
    do {                                                                       \
        size_t next = (rbuf)->head + 1;                                        \
        if(next >= (rbuf)->nelem)                                              \
            next = 0;                                                          \
        if(next == (rbuf)->tail) {                                             \
            (rbuf)->tail++;                                                    \
            if((rbuf)->tail >= (rbuf)->nelem)                                  \
                (rbuf)->tail = 0;                                              \
        }                                                                      \
        (rbuf)->buf[(rbuf)->head] = val;                                       \
        (rbuf)->head = next;                                                   \
    } while(0)

#define RBUF_TRY_PUSH(rbuf, val)                                               \
    ({                                                                         \
        size_t result = false;                                                 \
        size_t next = (rbuf)->head + 1;                                        \
        if(next >= (rbuf)->nelem)                                              \
            next = 0;                                                          \
        if(next != (rbuf)->tail) {                                             \
            (rbuf)->buf[(rbuf)->head] = val;                                   \
            (rbuf)->head = next;                                               \
            result = true;                                                     \
        }                                                                      \
        result;                                                                \
    })

#define RBUF_POP(rbuf, defvalue)                                               \
    ({                                                                         \
        typeof(defvalue) result = defvalue;                                    \
        if((rbuf)->head != (rbuf)->tail) {                                     \
            size_t next = (rbuf)->tail + 1;                                    \
            result = (rbuf)->buf[(rbuf)->tail];                                \
            (rbuf)->tail = next >= (rbuf)->nelem ? 0 : next;                   \
        }                                                                      \
        result;                                                                \
    })

#define RBUF_TRY_POP(rbuf, value)                                              \
    ({                                                                         \
        typeof(value) result;                                                  \
        if((rbuf)->head == (rbuf)->tail) {                                     \
            result = NULL;                                                     \
        } else {                                                               \
            size_t next = (rbuf)->tail + 1;                                    \
            *(value) = (rbuf)->buf[(rbuf)->tail];                              \
            (rbuf)->tail = next >= (rbuf)->nelem ? 0 : next;                   \
            result = value;                                                    \
        }                                                                      \
        result;                                                                \
    })

void test_rbuf(void);
