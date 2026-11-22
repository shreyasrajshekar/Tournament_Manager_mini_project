#include "core/ranking.h"
#include "util/util.h"

int ranking_cmp_power(const void *x, const void *y) {
    const Team *a = x, *b = y;
    if (a->rating > b->rating) return -1;
    if (a->rating < b->rating) return 1;
    return a->id - b->id;
}
static int cmp_standing(const void *x, const void *y) {
    const Team *a = x, *b = y;
    int ka = a->reached * 2 + (a->alive ? 1 : 0), kb = b->reached * 2 + (b->alive ? 1 : 0);
    if (ka != kb) return kb - ka;
    int da = a->pointsFor - a->pointsAgainst, db = b->pointsFor - b->pointsAgainst;
    if (da != db) return db - da;
    if (a->wins != b->wins) return b->wins - a->wins;
    return ranking_cmp_power(a, b);
}
static int drain(Tournament *t, HeapCmp cmp, const char *label, Team **out, int cap) {
    int prev = trace_enable(0);
    Heap h; heap_init(&h, cmp, label);
    LIST_FOREACH(n, &t->teams) heap_push(&h, n->data);
    int k = 0; Team *tm;
    while (k < cap && (tm = heap_pop(&h))) out[k++] = tm;
    heap_free(&h);
    trace_enable(prev);
    return k;
}
int ranking_power(Tournament *t, Team **out, int cap) { return drain(t, ranking_cmp_power, "power", out, cap); }
int ranking_standings(Tournament *t, Team **out, int cap) { return drain(t, cmp_standing, "standings", out, cap); }
