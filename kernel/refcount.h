#pragma once

#define REFCOUNT() int refcount

#define REFCOUNT_DECLARE(NAME, TYPE)                                          \
    int NAME##_refcount(struct TYPE*);                                        \
    int NAME##_incref(struct TYPE*);                                          \
    int NAME##_decref(struct TYPE*)

#define REFCOUNT_IMPLEMENT(NAME, STRUCT, DESTRUCTOR)                          \
    int NAME##_refcount(struct STRUCT* obj)                                   \
    {                                                                         \
        return obj->refcount;                                                 \
    }                                                                         \
                                                                              \
    int NAME##_incref(struct STRUCT* obj)                                     \
    {                                                                         \
        enter_critical_section();                                             \
        int result = ++obj->refcount;                                         \
        leave_critical_section();                                             \
        return result;                                                        \
    }                                                                         \
                                                                              \
    int NAME##_decref(struct STRUCT* obj)                                     \
    {                                                                         \
        enter_critical_section();                                             \
        assert(obj->refcount > 0);                                            \
        obj->refcount--;                                                      \
        if (obj->refcount) {                                                  \
            int result = obj->refcount;                                       \
            leave_critical_section();                                         \
            return result;                                                    \
        }                                                                     \
        leave_critical_section();                                             \
        (DESTRUCTOR)(obj);                                                    \
        return 0;                                                             \
    }

