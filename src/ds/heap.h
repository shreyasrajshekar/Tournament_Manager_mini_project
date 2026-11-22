/* heap.h - binary heap / priority queue with a user comparator.
 * cmp(a,b) < 0  means  a has HIGHER priority (comes out first).
 * Used for: ready-match queue (round, position), venue availability
 * (earliest free court), team seeding, leaderboards, "up next" ordering.
 * Owner: Member A */
#ifndef HEAP_H
#define HEAP_H
#include <stddef.h>

typedef int (*HeapCmp)(const void *a, const void *b);
typedef struct { void **a; size_t size, cap; HeapCmp cmp; const char *label; } Heap;

void   heap_init(Heap *h, HeapCmp cmp, const char *label);
int    heap_push(Heap *h, void *item);   /* O(log n); returns #sift-up swaps */
void  *heap_pop(Heap *h);                /* O(log n); NULL if empty */
void  *heap_peek(const Heap *h);
size_t heap_size(const Heap *h);
void   heap_clear(Heap *h);
void   heap_free(Heap *h);

#endif
