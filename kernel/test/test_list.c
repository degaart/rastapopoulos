#include "../debug.h"
#include "../util.h"
#include "../pmm.h"
#include "../vmm.h"
#include "../kernel.h"
#include "../string.h"
#include "../gdt.h"
#include "../context.h"
#include "../kmalloc.h"
#include "../registers.h"
#include "../timer.h"
#include "../io.h"
#include "../locks.h"

/* ( ͡° ͜ʖ ͡°)*/
struct list_node {
    struct list_node* next;
    struct list_node* prev;
    void* data;
};

struct list {
    struct list_node* head;
    struct list_node* tail;
};

void list_init(struct list* list)
{
    bzero(list, sizeof(struct list));
}

struct list_node* list_append(struct list* list, void* data)
{
    struct list_node* node = kmalloc(sizeof(struct list_node));
    node->next = NULL;
    node->prev = list->tail;
    node->data = data;

    if(list->tail) {
        list->tail->next = node;
        list->tail = node;
    } else {
        assert(!list->tail);
        list->head = list->tail = node;
    }
    return node;
}

struct list_node* list_push(struct list* list, void* data)
{
    struct list_node* node = kmalloc(sizeof(struct list_node));
    node->prev = NULL;
    node->next = list->head;
    node->data = data;

    if(list->head) {
        list->head->prev = node;
        list->head = node;
    } else {
        assert(!list->tail);
        list->head = list->tail = node;
    }

    return node;
}

void list_remove(struct list* list, struct list_node* node)
{
    if(node->prev)
        node->prev->next = node->next;
    if(node->next)
        node->next->prev = node->prev;
    if(list->head == node)
        list->head = node->next;
    if(list->tail == node)
        list->tail = node->prev;
}

void* list_pop(struct list* list)
{
    void* result = NULL;
    if(list->head) {
        struct list_node* node = list->head;
        result = node->data;
        if(node->next) {
            node->next->prev = NULL;
        }
        list->head = node->next;

        if(list->tail == node)
            list->tail = node->prev;

        kfree(node);
    }
    return result;
}

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
}

void test_list()
{
    trace("Testing lists");

    test_append();
    test_push();
    test_remove();
    test_pop();
}



