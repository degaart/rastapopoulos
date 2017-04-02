#include "../debug.h"
#include "../string.h"
#include "../kmalloc.h"
#include "../list.h"

struct element {
    unsigned val;
    list_declare_node(element) node;
};
list_declare(list, element);

static void test_append()
{
    trace("Testing list_append");

    struct list l = {0};

    struct element elements[3] = {0};
    elements[0].val = 0xABCDEF00;
    elements[1].val = 0xABCDEF01;
    elements[2].val = 0xABCDEF02;

    list_append(&l, &elements[0], node);
    list_append(&l, &elements[1], node);
    list_append(&l, &elements[2], node);

    struct element* e = list_head(&l);
    assert(e->val == 0xABCDEF00);

    e = list_next(e, node);
    assert(e->val == 0xABCDEF01);

    e = list_next(e, node);
    assert(e->val == 0xABCDEF02);
}

static void test_foreach()
{
    trace("Testing list_foreach");

    struct list l = {0};

    struct element elements[3] = {0};
    elements[0].val = 0xABCDEF00;
    elements[1].val = 0xABCDEF01;
    elements[2].val = 0xABCDEF02;

    list_append(&l, &elements[0], node);
    list_append(&l, &elements[1], node);
    list_append(&l, &elements[2], node);

    int index = 0;
    list_foreach(element, e, &l, node) {
        switch(index) {
            case 0:
                assert(e->val == 0xABCDEF00);
                break;
            case 1:
                assert(e->val == 0xABCDEF01);
                break;
            case 2:
                assert(e->val == 0xABCDEF02);
                break;
        }
        index++;
    }
}

static void test_remove()
{
    trace("Testing list_remove");

    struct list l = {0};

    struct element elements[5] = {0};
    elements[0].val = 0xABCDEF00;
    elements[1].val = 0xABCDEF01;
    elements[2].val = 0xABCDEF02;
    elements[3].val = 0xABCDEF03;
    elements[4].val = 0xABCDEF04;

    do {
        if(list_empty(&l)) {
            (&l)->tail = (&l)->head = &elements[0];
        } else {
            (&l)->tail->node.next = &elements[0];
            (&elements[0])->node.prev = (&l)->tail;
            (&elements[0])->node.next = NULL;
            (&l)->tail = &elements[0];
        }
    } while(0);

    //list_append(&l, &elements[0], node);
    list_append(&l, &elements[1], node);
    list_append(&l, &elements[2], node);
    list_append(&l, &elements[3], node);
    list_append(&l, &elements[4], node);

    /* Test removing from middle of list */
    list_remove(&l, &elements[2], node);
    struct element* e = list_head(&l);
    assert(e->val == 0xABCDEF00);

    e = list_next(e, node);
    assert(e->val == 0xABCDEF01);

    e = list_next(e, node);
    assert(e->val == 0xABCDEF03);

    e = list_next(e, node);
    assert(e->val == 0xABCDEF04);

    e = list_next(e, node);
    assert(e == NULL);

    /* Test removing just after first element */
    list_remove(&l, &elements[1], node);
    e = list_head(&l);
    assert(e->val == 0xABCDEF00);
    e = list_next(e, node);
    assert(e->val == 0xABCDEF03);
    e = list_next(e, node);
    assert(e->val == 0xABCDEF04);
    e = list_next(e, node);
    assert(e == NULL);

    /* Test removing tail */
    list_remove(&l, &elements[4], node);
    e = list_head(&l);
    assert(e->val == 0xABCDEF00);
    e = list_next(e, node);
    assert(e->val == 0xABCDEF03);
    e = list_next(e, node);
    assert(e == NULL);

    /* Test removing head */
    list_remove(&l, &elements[0], node);
    e = list_head(&l);
    assert(e->val == 0xABCDEF03);
    e = list_next(e, node);
    assert(e == NULL);

    /* Test removing head when it's the only element in the list */
    list_remove(&l, &elements[3], node);
    e = list_head(&l);
    assert(e == NULL);
    e = list_tail(&l);
    assert(e == NULL);

    /* Test removing tail when it's the only element */
    list_init(&l);
    list_append(&l, &elements[0], node);

    list_remove(&l, &elements[0], node);
    e = list_head(&l);
    assert(e == NULL);
    e = list_tail(&l);
    assert(e == NULL);
}

void test_list()
{
    trace("Testing lists");

    test_append();
    test_foreach();
    test_remove();
}


