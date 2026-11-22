/* queue.h - growable circular FIFO queue of void*.
 * Used for: building the bracket tree level by level, and level-order
 * (BFS) traversal of the bracket for export.
 * Owner: Member A */
#ifndef QUEUE_H
#define QUEUE_H
#include <stddef.h>

typedef struct { void **buf; size_t head, size, cap; } Queue;

void   queue_init(Queue *q);
void   queue_push(Queue *q, void *item);   /* enqueue, amortised O(1) */
void  *queue_pop(Queue *q);                /* dequeue, O(1); NULL if empty */
void  *queue_peek(const Queue *q);
size_t queue_size(const Queue *q);
void   queue_free(Queue *q);

#endif
