#include "ds/heap.h"
#include "util/util.h"
#include <stdlib.h>

void heap_init(Heap *h, HeapCmp cmp, const char *label) {
    h->a = NULL; h->size = h->cap = 0; h->cmp = cmp; h->label = label;
}
static void swap(Heap *h, size_t i, size_t j) { void *t = h->a[i]; h->a[i] = h->a[j]; h->a[j] = t; }

int heap_push(Heap *h, void *item) {
    if (h->size == h->cap) { h->cap = h->cap ? h->cap * 2 : 16; h->a = xrealloc(h->a, h->cap * sizeof(void *)); }
    size_t i = h->size++;
    h->a[i] = item;
    int swaps = 0;
    while (i > 0) {
        size_t p = (i - 1) / 2;
        if (h->cmp(h->a[i], h->a[p]) < 0) { swap(h, i, p); i = p; swaps++; } else break;
    }
    trace_log("[HEAP:%s] push -> slot %zu (sift-up swaps=%d, size=%zu)", h->label, i, swaps, h->size);
    return swaps;
}
void *heap_pop(Heap *h) {
    if (!h->size) return NULL;
    void *top = h->a[0];
    h->a[0] = h->a[--h->size];
    size_t i = 0; int swaps = 0;
    for (;;) {
        size_t l = 2 * i + 1, r = l + 1, s = i;
        if (l < h->size && h->cmp(h->a[l], h->a[s]) < 0) s = l;
        if (r < h->size && h->cmp(h->a[r], h->a[s]) < 0) s = r;
        if (s == i) break;
        swap(h, i, s); i = s; swaps++;
    }
    trace_log("[HEAP:%s] pop min (sift-down swaps=%d, size=%zu)", h->label, swaps, h->size);
    return top;
}
void *heap_peek(const Heap *h) { return h->size ? h->a[0] : NULL; }
size_t heap_size(const Heap *h) { return h->size; }
void heap_clear(Heap *h) { h->size = 0; }
void heap_free(Heap *h) { free(h->a); h->a = NULL; h->size = h->cap = 0; }
