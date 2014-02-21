#ifndef _LL_H_
#define _LL_H_

#include <stddef.h>     /* for offsetof */
#include "kutil.h"      /* for STATIC_ASSERT */

/*
 Very simple singly linked list implementation
 el_type must have a member el_type* next
 */
#define LL_HEADER(el_type) \
    struct el_type* previous; \
    struct el_type* next

struct LL_ELEMENT {
    LL_HEADER(LL_ELEMENT);
    int value;
};

struct LL {
    struct LL_ELEMENT* first;
    struct LL_ELEMENT* last;
};

void ll_init(struct LL* lst);
void ll_append(struct LL* lst, struct LL_ELEMENT* el);
void ll_remove(struct LL* lst, struct LL_ELEMENT* el);
void ll_insert(struct LL* lst, struct LL_ELEMENT* previous, struct LL_ELEMENT* el);
void ll_clear(struct LL* lst);
int ll_empty(struct LL* lst);

#define LL_DECLARE(typename, el_type) \
    struct typename { \
        struct el_type* first; \
        struct el_type* last; \
    }; \
    void typename##_init(struct typename* lst); \
    void typename##_append(struct typename* lst, struct el_type* el); \
    void typename##_remove(struct typename* lst, struct el_type* el); \
    void typename##_insert(struct typename* lst, struct el_type* previous, struct el_type* el); \
    void typename##_clear(struct typename* lst); \
    int typename##_empty(struct typename* lst)

#define LL_IMPLEMENT(typename, el_type) \
    STATIC_ASSERT( (offsetof(struct el_type, previous) == 0) ); \
    STATIC_ASSERT( (offsetof(struct el_type, next) == 4) ); \
    void typename##_init(struct typename* lst) { ll_init((struct LL*)lst); } \
    void typename##_append(struct typename* lst, struct el_type* el) { ll_append((struct LL*)lst, (struct LL_ELEMENT*)el); } \
    void typename##_remove(struct typename* lst, struct el_type* el) { ll_remove((struct LL*)lst, (struct LL_ELEMENT*)el);  } \
    void typename##_insert(struct typename* lst, struct el_type* previous, struct el_type* el) { ll_insert((struct LL*)lst, (struct LL_ELEMENT*)previous, (struct LL_ELEMENT*)el); } \
    void typename##_clear(struct typename* lst) { ll_clear((struct LL*)lst); } \
    int typename##_empty(struct typename* lst) { return(ll_empty((struct LL*)lst)); }



#endif // _LL_H_


