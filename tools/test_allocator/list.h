#pragma once

#define list_declare(name, elem)                                                \
    struct name {                                                               \
        struct elem* head;                                                      \
        struct elem* tail;                                                      \
    }

#define list_declare_node(elem)                                                 \
    struct {                                                                    \
        struct elem* next;                                                      \
        struct elem* prev;                                                      \
    }

#define list_init(lst)                                                          \
    do {                                                                        \
        (lst)->head = NULL;                                                     \
        (lst)->tail = NULL;                                                     \
    } while(0)

#define list_head(lst)                                                          \
    (lst)->head

#define list_tail(lst)                                                          \
    (lst)->tail

#define list_next(elem, node)                                                   \
    (elem)->node.next

#define list_prev(elem, node)                                                   \
    (elem)->node.prev

#define list_foreach(elem, it, lst, node)                                       \
    for(struct elem *it = list_head(lst),                                       \
        *list_foreach_next = it ? list_next(it, node) : NULL;                   \
        it != NULL;                                                             \
        it = list_foreach_next,                                                 \
        list_foreach_next = list_foreach_next ?                                 \
            list_next(list_foreach_next, node) :                                \
            NULL)

#define list_remove(lst, elem, node)                                            \
    do {                                                                        \
        if(list_prev(elem, node))                                               \
            list_next(list_prev(elem, node), node) =                            \
                list_next(elem, node);                                          \
        if(list_next(elem, node))                                               \
            list_prev(list_next(elem, node), node) =                            \
                list_prev(elem, node);                                          \
        if(list_head(lst) == (elem))                                            \
            list_head(lst) = list_next(elem, node);                             \
        if(list_tail(lst) == (elem))                                            \
            list_tail(lst) = list_prev(elem, node);                             \
        list_prev(elem, node) = NULL;                                           \
        list_next(elem, node) = NULL;                                           \
    } while(0)

#define list_append(lst, elem, node)                                            \
    do {                                                                        \
        if(list_empty(lst)) {                                                   \
            (lst)->tail = (lst)->head = elem;                                   \
        } else {                                                                \
            (lst)->tail->node.next = elem;                                      \
            (elem)->node.prev = (lst)->tail;                                    \
            (elem)->node.next = NULL;                                           \
            (lst)->tail = elem;                                                 \
        }                                                                       \
    } while(0)

#define list_insert_after(lst, elem, prev, node) \
    do { \
        assert(!list_empty(lst)); \
        list_prev(elem, node) = prev; \
        list_next(elem, node) = list_next(prev, node); \
        list_next(prev, node) = elem; \
    } while(0)

#define list_insert(lst, elem, prev, node) \
    list_insert_after(lst, elem, prev, node)

#define list_insert_before(lst, elem, next, node) \
    do { \
        assert(!list_empty(lst)); \
        list_next(elem, node) = next; \
        list_prev(elem, node) = list_prev(next, node); \
        list_prev(next, node) = elem; \
    } while(0)


#define list_empty(lst)                                                         \
    ((lst)->head == NULL)



