#include "ds/queue.h"
#include "util/util.h"
#include <stdlib.h>

void queue_init(Queue *q) { q->cap = 8; q->buf = xmalloc(q->cap * sizeof(void *)); q->head = q->size = 0; }

void queue_push(Queue *q, void *item) {
    if (q->size == q->cap) {                       /* grow + unwrap the ring */
        void **nb = xmalloc(q->cap * 2 * sizeof(void *));
        for (size_t i = 0; i < q->size; i++) nb[i] = q->buf[(q->head + i) % q->cap];
        free(q->buf); q->buf = nb; q->head = 0; q->cap *= 2;
    }
    q->buf[(q->head + q->size) % q->cap] = item;
    q->size++;
}
void *queue_pop(Queue *q) {
    if (!q->size) return NULL;
    void *it = q->buf[q->head];
    q->head = (q->head + 1) % q->cap; q->size--;
    return it;
}
void *queue_peek(const Queue *q) { return q->size ? q->buf[q->head] : NULL; }
size_t queue_size(const Queue *q) { return q->size; }
void queue_free(Queue *q) { free(q->buf); q->buf = NULL; q->size = q->head = q->cap = 0; }
