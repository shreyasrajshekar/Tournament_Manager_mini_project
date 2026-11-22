#include "ds/linkedlist.h"
#include "util/util.h"
#include <stdlib.h>

void list_init(List *l) { l->head = l->tail = NULL; l->size = 0; }

ListNode *list_push_back(List *l, void *data) {
    ListNode *n = xmalloc(sizeof *n);
    n->data = data; n->next = NULL; n->prev = l->tail;
    if (l->tail) l->tail->next = n; else l->head = n;
    l->tail = n; l->size++;
    return n;
}
ListNode *list_push_front(List *l, void *data) {
    ListNode *n = xmalloc(sizeof *n);
    n->data = data; n->prev = NULL; n->next = l->head;
    if (l->head) l->head->prev = n; else l->tail = n;
    l->head = n; l->size++;
    return n;
}
void *list_remove(List *l, ListNode *n) {
    void *d = n->data;
    if (n->prev) n->prev->next = n->next; else l->head = n->next;
    if (n->next) n->next->prev = n->prev; else l->tail = n->prev;
    free(n); l->size--;
    return d;
}
void *list_pop_back(List *l)  { return l->tail ? list_remove(l, l->tail) : NULL; }
void *list_pop_front(List *l) { return l->head ? list_remove(l, l->head) : NULL; }
void list_clear(List *l, void (*free_fn)(void *)) {
    ListNode *n = l->head;
    while (n) { ListNode *nx = n->next; if (free_fn) free_fn(n->data); free(n); n = nx; }
    l->head = l->tail = NULL; l->size = 0;
}
