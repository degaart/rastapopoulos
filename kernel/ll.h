#ifndef _LL_H_
#define _LL_H_

/*
 Very simple singly linked list implementation
 el_type must have a member el_type* next
*/

#define LL_DECLARE(typename, el_type) \
    typedef struct typename##_t { \
        el_type* first; \
    } typename; \
    \
    void typename##_init(typename*); \
    void typename##_append(typename*, el_type*) \



#define LL_IMPLEMENT(typename, el_type) \
    void typename##_init(typename* lst) { \
        lst->first = 0; \
    } \
    \
    void typename##_append(typename* lst, el_type* el) { \
        el_type* last_entry = 0; \
        if(lst->first) { \
            for(last_entry=lst->first; last_entry->next; last_entry=last_entry->next); \
        } \
        \
        if(lst->first) \
            last_entry->next = el; \
        else \
            lst->first = el; \
        el->next = 0; \
    }

#endif //_LL_H_


