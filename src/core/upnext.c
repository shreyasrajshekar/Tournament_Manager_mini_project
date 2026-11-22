#include "core/upnext.h"
#include "util/util.h"

static int cmp_order(const void *x, const void *y) {
    const Match *a = x, *b = y;
    if (a->round != b->round) return a->round - b->round;
    return a->pos - b->pos;
}

int upnext_list(Tournament *t, Match **out, int cap) {
    int prev = trace_enable(0);
    Heap h; heap_init(&h, cmp_order, "upnext");
    for (int i = 1; i <= t->matchCount; i++) if (t->matches[i]->status == M_READY) heap_push(&h, t->matches[i]);
    int n = 0; Match *m;
    while (n < cap && (m = heap_pop(&h))) out[n++] = m;
    heap_free(&h);
    trace_enable(prev);
    return n;
}
