#include "../debug.h"
#include "../string.h"
#include "../kmalloc.h"
#include "../list.h"

static void test_append()
{
    trace("Testing list_append");

    struct list l;
    list_init(&l);

    struct list_node* nodes[3];
    nodes[0] = list_append(&l, (void*)0x4C00);
    assert(nodes[0]->prev == NULL);
    assert(nodes[0]->next == NULL);
    assert(l.head == nodes[0]);
    assert(l.tail == nodes[0]);

    nodes[1] = list_append(&l, (void*)0x4C01);
    assert(nodes[1]->prev == nodes[0]);
    assert(nodes[1]->next == NULL);
    assert(l.head == nodes[0]);
    assert(l.tail == nodes[1]);

    nodes[2] = list_append(&l, (void*)0x4C02);
    assert(nodes[2]->prev == nodes[1]);
    assert(nodes[2]->next == NULL);
    assert(l.head == nodes[0]);
    assert(l.tail == nodes[2]);

    assert(nodes[0]->prev == NULL);
    assert(nodes[0]->next == nodes[1]);
    assert(nodes[1]->prev == nodes[0]);
    assert(nodes[1]->next == nodes[2]);
    assert(nodes[2]->prev == nodes[1]);
    assert(nodes[2]->next == NULL);

    struct list_node* n = l.head;
    assert(n == nodes[0]);
    assert(n->data == (void*)0x4C00);

    n = n->next;
    assert(n == nodes[1]);
    assert(n->data == (void*)0x4C01);

    n = n->next;
    assert(n == nodes[2]);
    assert(n->data == (void*)0x4C02);

    n = n->next;
    assert(n == NULL);

    list_destroy(&l, NULL);
}

static void test_push()
{
    trace("Testing list_push");

    struct list l;
    list_init(&l);

    struct list_node* nodes[3];
    nodes[0] = list_push(&l, (void*)0x4C00);
    assert(nodes[0]->prev == NULL);
    assert(nodes[0]->next == NULL);
    assert(l.head == nodes[0]);
    assert(l.tail == nodes[0]);

    nodes[1] = list_push(&l, (void*)0x4C01);
    assert(nodes[0]->prev == nodes[1]);
    assert(nodes[0]->next == NULL);
    assert(nodes[1]->prev == NULL);
    assert(nodes[1]->next == nodes[0]);
    assert(l.head == nodes[1]);
    assert(l.tail == nodes[0]);

    nodes[2] = list_push(&l, (void*)0x4C02);
    assert(nodes[0]->prev == nodes[1]);
    assert(nodes[0]->next == NULL);
    assert(nodes[1]->prev == nodes[2]);
    assert(nodes[1]->next == nodes[0]);
    assert(nodes[2]->prev == NULL);
    assert(nodes[2]->next == nodes[1]);
    assert(l.head == nodes[2]);
    assert(l.tail == nodes[0]);

    struct list_node* n = l.head;
    assert(n->data == (void*)0x4C02);

    n = n->next;
    assert(n->data == (void*)0x4C01);

    n = n->next;
    assert(n->data == (void*)0x4C00);

    n = n->next;
    assert(n == NULL);

    list_destroy(&l, NULL);
}

static void test_remove()
{
    trace("Testing list_remove");

    struct list l;
    list_init(&l);

    struct list_node* nodes[5];

    nodes[0] = list_append(&l, (void*)0x4C00);
    nodes[1] = list_append(&l, (void*)0x4C01);
    nodes[2] = list_append(&l, (void*)0x4C02);
    nodes[3] = list_append(&l, (void*)0x4C03);
    nodes[4] = list_append(&l, (void*)0x4C04);

    /* Test delete from middle */
    struct list_node* n;
    list_remove(&l, nodes[2]);

    assert(nodes[1]->next == nodes[3]);
    assert(nodes[3]->prev == nodes[1]);

    n = l.head;
    assert(n->data == (void*)0x4C00);

    n = n->next;
    assert(n->data == (void*)0x4C01);

    n = n->next;
    assert(n->data == (void*)0x4C03);

    n = n->next;
    assert(n->data == (void*)0x4C04);

    n = n->next;
    assert(n == NULL);

    /* Test delete head */
    list_remove(&l, l.head);

    assert(nodes[1]->prev == NULL);
    assert(l.head == nodes[1]);
    assert(l.tail == nodes[4]);

    n = l.head;
    assert(n->data == (void*)0x4C01);

    n = n->next;
    assert(n->data == (void*)0x4C03);

    n = n->next;
    assert(n->data == (void*)0x4C04);

    n = n->next;
    assert(n == NULL);

    /* Delete tail */
    list_remove(&l, l.tail);

    assert(nodes[3]->next == NULL);
    assert(l.head == nodes[1]);
    assert(l.tail == nodes[3]);

    n = l.head;
    assert(n->data == (void*)0x4C01);

    n = n->next;
    assert(n->data == (void*)0x4C03);

    n = n->next;
    assert(n == NULL);

    /* Delete remaining items */
    list_remove(&l, l.head);
    
    assert(nodes[3]->prev == NULL);
    assert(nodes[3]->next == NULL);
    assert(l.head == nodes[3]);
    assert(l.tail == nodes[3]);

    n = l.head;
    assert(n->data == (void*)0x4C03);

    n = n->next;
    assert(n == NULL);

    list_remove(&l, l.head);

    n = l.head;
    assert(n == NULL);

    list_destroy(&l, NULL);
}

static void test_pop()
{
    trace("Testing list_pop");

    struct list l;
    list_init(&l);

    struct list_node* nodes[4];
    nodes[0] = list_append(&l, (void*)0x4C00);
    nodes[1] = list_append(&l, (void*)0x4C01);
    nodes[2] = list_append(&l, (void*)0x4C02);
    nodes[3] = list_append(&l, (void*)0x4C03);

    void* data = list_pop(&l);
    assert(nodes[1]->prev == NULL);
    assert(l.head == nodes[1]);
    assert(l.tail == nodes[3]);
    assert(data == (void*)0x4C00);

    data = list_pop(&l);
    assert(nodes[2]->prev == NULL);
    assert(l.head == nodes[2]);
    assert(data == (void*)0x4C01);

    data = list_pop(&l);
    assert(nodes[3]->prev == NULL);
    assert(l.head == nodes[3]);
    assert(data == (void*)0x4C02);

    data = list_pop(&l);
    assert(l.head == NULL);
    assert(l.tail == NULL);
    assert(data == (void*)0x4C03);

    assert(l.head == NULL);
    assert(l.tail == NULL);
    data = list_pop(&l);
    assert(data == NULL);

    list_destroy(&l, NULL);
}

void test_list()
{
    trace("Testing lists");

    test_append();
    test_push();
    test_remove();
    test_pop();
}



