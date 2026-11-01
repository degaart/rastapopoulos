#pragma once

#define REFCOUNT() int refcount

#define REFCOUNT_DECLARE(NAME, TYPE)                                          \
    int NAME##_refcount(struct TYPE*);                                        \
    struct TYPE* NAME##_incref(struct TYPE*);                                 \
    struct TYPE* NAME##_decref(struct TYPE*)

#define REFCOUNT_IMPLEMENT(NAME, STRUCT, DESTRUCTOR)                          \
    int NAME##_refcount(struct STRUCT* obj)                                   \
    {                                                                         \
        if (!obj)                                                             \
            return 0;                                                         \
        return obj->refcount;                                                 \
    }                                                                         \
                                                                              \
    struct STRUCT* NAME##_incref(struct STRUCT* obj)                          \
    {                                                                         \
        if (!obj)                                                             \
            return NULL;                                                      \
        enter_critical_section();                                             \
        ++obj->refcount;                                                      \
        leave_critical_section();                                             \
        return obj;                                                           \
    }                                                                         \
                                                                              \
    struct STRUCT* NAME##_decref(struct STRUCT* obj)                          \
    {                                                                         \
        if (!obj)                                                             \
            return NULL;                                                      \
        enter_critical_section();                                             \
        assert(obj->refcount > 0);                                            \
        obj->refcount--;                                                      \
        if (obj->refcount) {                                                  \
            leave_critical_section();                                         \
            return obj;                                                       \
        }                                                                     \
        leave_critical_section();                                             \
        (DESTRUCTOR)(obj);                                                    \
        return NULL;                                                          \
    }

