#ifdef __APPLE__
#include <stdio.h>
#include <stdlib.h>
#endif
#include <stdint.h>
#include "kstring.h"
#include "kutil.h"
#include "ll.h"

void ll_init(struct LL* lst) {
    ll_clear(lst);
}

void ll_append(struct LL* lst, struct LL_ELEMENT* el) {
    ASSERT(el != 0);
    
    if(!lst->first) {
        lst->first = el;
        el->previous = 0;
    } else {
        lst->last->next = el;
        el->previous = lst->last;
    }
    lst->last = el;
    el->next = 0;
}

void ll_insert(struct LL* lst, struct LL_ELEMENT* previous, struct LL_ELEMENT* el) {
    ASSERT(el != 0);
    ASSERT(el != previous);

    if(!lst->first) {
        ll_append(lst, el);
        return;
    }
   
    if(previous) {
        el->previous = previous;
        el->next = previous->next;
        previous->next = el;
        if(previous == lst->last)
            lst->last = el;
    } else {
        /* insert at start of list */
        el->next = lst->first;
        lst->first->previous = el;
        el->previous = 0;
        lst->first = el;
    }
}

void ll_remove(struct LL* lst, struct LL_ELEMENT* el) {
    ASSERT(el != 0);
    //ASSERT(el->previous || el->next);
    
    if(el->previous)
        el->previous->next = el->next;
    else
        lst->first = el->next;

    if(el->next)
        el->next->previous = el->previous;
    else
        lst->last = el->previous;
}

void ll_clear(struct LL* lst) {
    bzero(lst, sizeof(struct LL));
}

int ll_empty(struct LL* lst) {
    return(lst->first != 0);
}


