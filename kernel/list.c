#include "list.h"
#include "debug.h"
#include "kmalloc.h"
#include "string.h"

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

    kfree(node);
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

void list_destroy(struct list* list, void (*data_destructor)(void*))
{
    struct list_node* node = list->head;
    while(node) {
        struct list_node* next = node->next;
        if(data_destructor)
            data_destructor(node->data);
        kfree(node);
        node = next;
    }
}



