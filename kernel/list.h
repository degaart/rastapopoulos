#pragma once

struct list_node {
    struct list_node* next;
    struct list_node* prev;
    void* data;
};

struct list {
    struct list_node* head;
    struct list_node* tail;
};


void list_init(struct list* list);
struct list_node* list_append(struct list* list, void* data);
struct list_node* list_push(struct list* list, void* data);
void list_remove(struct list* list, struct list_node* node);
void* list_pop(struct list* list);
void list_destroy(struct list* list, void (*data_destructor)(void*));
static void test_append();


