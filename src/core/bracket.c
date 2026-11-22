#include "core/bracket.h"
#include "core/history.h"
#include "util/util.h"
#include <stdio.h>
#include <stdlib.h>

/* ---------- seeding: max-heap on rating ---------- */
static int cmp_seed(const void *x, const void *y) {
    const Team *a = x, *b = y;
    if (a->rating0 > b->rating0) return -1;
    if (a->rating0 < b->rating0) return 1;
    return a->id - b->id;
}
void bracket_seed_teams(Tournament *t) {
    Heap h; heap_init(&h, cmp_seed, "seeding");
    LIST_FOREACH(n, &t->teams) heap_push(&h, n->data);
    int s = 1; Team *tm;
    while ((tm = heap_pop(&h))) tm->seed = s++;
    heap_free(&h);
}

/* Standard seeding order so #1 and #2 can only meet in the final: P=8 -> 1 8 4 5 2 7 3 6 */
static void seed_order(int P, int *out) {
    out[0] = 1;
    for (int n = 1; n < P; n *= 2)
        for (int i = n - 1; i >= 0; i--) { out[2 * i + 1] = 2 * n + 1 - out[i]; out[2 * i] = out[i]; }
}

static Match *new_match(Tournament *t, int round, int pos) {
    Match *m = xcalloc(1, sizeof *m);
    m->id = ++t->matchCount; m->round = round; m->pos = pos;
    m->status = M_PENDING;
    t->matches[m->id] = m;
    return m;
}

void bracket_free(Tournament *t) {
    if (t->matches) { for (int i = 1; i <= t->matchCount; i++) free(t->matches[i]); free(t->matches); }
    t->matches = NULL; t->root = NULL; t->matchCount = 0; t->rounds = 0; t->bracketSize = 0;
}

static void make_ready(Match *m) {
    m->status = M_READY;
    trace_log("[TREE] match #%d is ready: %s vs %s", m->id, m->a->name, m->b->name);
}

void bracket_advance(Tournament *t, Match *m) {
    (void)t;
    Match *p = m->parent;
    if (!p) return;
    if (p->left == m) p->a = m->winner; else p->b = m->winner;
    if (m->winner->reached < p->round) m->winner->reached = p->round;
    trace_log("[TREE] %s climbs to match #%d (round %d) via parent pointer", m->winner->name, p->id, p->round);
    if (p->a && p->b) make_ready(p);
}

void bracket_build(Tournament *t) {
    int N = (int)t->teams.size, P = 1;
    while (P < N) P *= 2;
    Team *bySeed[MAX_TEAMS + 1] = { 0 };
    LIST_FOREACH(n, &t->teams) { Team *tm = n->data; bySeed[tm->seed] = tm; }
    int *order = xmalloc((size_t)P * sizeof(int));
    seed_order(P, order);

    t->bracketSize = P; t->matchCount = 0;
    t->matches = xcalloc((size_t)P + 1, sizeof(Match *));

    Queue q; queue_init(&q);
    for (int i = 0; i < P / 2; i++) {
        Match *m = new_match(t, 1, i);
        int sa = order[2 * i], sb = order[2 * i + 1];
        m->a = sa <= N ? bySeed[sa] : NULL;
        m->b = sb <= N ? bySeed[sb] : NULL;
        queue_push(&q, m);
    }
    trace_log("[QUEUE] enqueued %d first-round matches (bracket size %d)", P / 2, P);
    while (queue_size(&q) > 1) {
        size_t level = queue_size(&q);
        for (size_t k = 0; k < level / 2; k++) {
            Match *l = queue_pop(&q), *r = queue_pop(&q);
            Match *p = new_match(t, l->round + 1, (int)k);
            p->left = l; p->right = r; l->parent = r->parent = p;
            queue_push(&q, p);
        }
    }
    t->root = queue_pop(&q);
    t->rounds = t->root->round;
    queue_free(&q); free(order);
    trace_log("[TREE] built binary tree: %d matches, %d rounds", t->matchCount, t->rounds);

    for (int i = 1; i <= P / 2; i++) {
        Match *m = t->matches[i];
        if (m->a) m->a->reached = 1;
        if (m->b) m->b->reached = 1;
        if (m->a && m->b) { make_ready(m); continue; }
        m->status = M_BYE; m->winner = m->a ? m->a : m->b; m->winner->byes++;
        event_log(t, "bye", "Seed %d %s receives a first-round bye", m->winner->seed, m->winner->name);
    }
    for (int i = 1; i <= P / 2; i++) if (t->matches[i]->status == M_BYE) bracket_advance(t, t->matches[i]);
}

int bracket_level_order(Tournament *t, Match **out, int cap) {
    if (!t->root) return 0;
    Queue q; queue_init(&q); queue_push(&q, t->root);
    int n = 0; Match *m;
    while ((m = queue_pop(&q))) {
        if (n < cap) out[n++] = m;
        if (m->left) queue_push(&q, m->left);
        if (m->right) queue_push(&q, m->right);
    }
    queue_free(&q);
    return n;
}

const char *bracket_round_name(const Tournament *t, int round, char *buf, size_t cap) {
    int from_end = t->rounds - round;
    if (from_end == 0) snprintf(buf, cap, "Final");
    else if (from_end == 1) snprintf(buf, cap, "Semi-finals");
    else if (from_end == 2) snprintf(buf, cap, "Quarter-finals");
    else snprintf(buf, cap, "Round of %d", 1 << (from_end + 1));
    return buf;
}
