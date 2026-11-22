/* linkedlist.h - doubly linked list of void*.
 * Used for: team registry (registration order), per-team match history,
 * tournament event log (append-only activity feed).
 * Owner: Member A */
#ifndef LINKEDLIST_H
#define LINKEDLIST_H
#include <stddef.h>

typedef struct ListNode { void *data; struct ListNode *prev, *next; } ListNode;
typedef struct { ListNode *head, *tail; size_t size; } List;

void      list_init(List *l);
ListNode *list_push_back(List *l, void *data);   /* O(1) */
ListNode *list_push_front(List *l, void *data);  /* O(1) */
void     *list_pop_back(List *l);                /* O(1) */
void     *list_pop_front(List *l);               /* O(1) */
void     *list_remove(List *l, ListNode *n);     /* O(1) given the node */
void      list_clear(List *l, void (*free_fn)(void *)); /* frees nodes (+data if fn) */

#define LIST_FOREACH(n, l) for (ListNode *n = (l)->head; n; n = n->next)

#endif
