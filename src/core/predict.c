#include "core/predict.h"
#include "core/history.h"
#include "core/tournament.h"
#include "core/team.h"
#include "util/util.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(char *err, size_t cap, const char *msg) { if (err) snprintf(err, cap, "%s", msg); return -1; }

Fan *fan_find(Tournament *t, const char *name) {
    char key[FAN_NAME_MAX + 1]; str_lower(key, sizeof key, name);
    return hashmap_get(&t->fanIndex, key);
}
Fan *fan_by_id(Tournament *t, int id) {
    LIST_FOREACH(n, &t->fans) if (((Fan *)n->data)->id == id) return n->data;
    return NULL;
}

int fan_add(Tournament *t, const char *name, char *err, size_t ecap) {
    char buf[FAN_NAME_MAX]; snprintf(buf, sizeof buf, "%s", name ? name : "");
    str_sanitize(buf); str_trim(buf);
    if (!*buf) return fail(err, ecap, "Fan name cannot be empty.");
    if (t->fans.size >= MAX_FANS) return fail(err, ecap, "Maximum of 64 fans reached.");
    char key[FAN_NAME_MAX + 1]; str_lower(key, sizeof key, buf);
    if (hashmap_get(&t->fanIndex, key)) return fail(err, ecap, "A fan with that name already exists.");
    Fan *f = xcalloc(1, sizeof *f);
    f->id = t->nextFanId++; snprintf(f->name, sizeof f->name, "%s", buf);
    hashmap_put(&t->fanIndex, key, f);
    list_push_back(&t->fans, f);
    trace_log("[HASH] index fan '%s'; [LIST] append to fan list (size=%zu)", f->name, t->fans.size);
    event_log(t, "fan", "%s joined the predictions game with %d points", f->name, FAN_START_POINTS);
    return 0;
}

/* P(pick wins) from Elo, clamped so payouts stay sensible. */
int predict_payout_for(const Tournament *t, const Match *m, int pick, int stake) {
    (void)t;
    double pa = elo_expected(m->a->rating, m->b->rating);
    double p = pick == m->a->id ? pa : 1.0 - pa;
    if (p < 0.15) p = 0.15;
    if (p > 0.85) p = 0.85;
    int pay = (int)floor(stake / p + 0.5);
    return pay <= stake ? stake + 1 : pay;
}

static void pred_key(char *buf, size_t cap, int fan, int match) { snprintf(buf, cap, "%d:%d", fan, match); }

static int store(Tournament *t, int fan, int match, int pick, int stake, int payout) {
    Prediction *p = xcalloc(1, sizeof *p);
    p->id = t->nextPredId++; p->fanId = fan; p->matchId = match; p->pickId = pick; p->stake = stake; p->payout = payout;
    char key[40]; pred_key(key, sizeof key, fan, match);
    hashmap_put(&t->predIndex, key, p);
    list_push_back(&t->predictions, p);
    return p->id;
}

int predict_place(Tournament *t, int fan_id, int match_id, int pick, int stake, char *err, size_t ecap) {
    if (t->phase != PHASE_RUNNING) return fail(err, ecap, "Predictions open once the bracket is running.");
    Fan *f = fan_by_id(t, fan_id);
    if (!f) return fail(err, ecap, "Fan not found.");
    if (match_id < 1 || match_id > t->matchCount) return fail(err, ecap, "Unknown match.");
    Match *m = t->matches[match_id];
    if (m->status != M_READY) return fail(err, ecap, m->status == M_DONE ? "That match is already played." : "That match has no clubs yet.");
    if (!m->a || !m->b || (pick != m->a->id && pick != m->b->id)) return fail(err, ecap, "Pick one of the two clubs playing.");
    if (stake < 1) return fail(err, ecap, "Stake at least 1 point.");
    char key[40]; pred_key(key, sizeof key, fan_id, match_id);
    if (hashmap_get(&t->predIndex, key)) return fail(err, ecap, "This fan already predicted that match.");
    int bal = fan_points(t, f);
    if (stake > bal) return fail(err, ecap, "Not enough points.");
    int pay = predict_payout_for(t, m, pick, stake);
    store(t, fan_id, match_id, pick, stake, pay);
    Team *pk = pick == m->a->id ? m->a : m->b;
    trace_log("[HASH] key '%s' checked: no earlier prediction; [LIST] append prediction", key);
    event_log(t, "predict", "%s backs %s in match #%d (stake %d, returns %d)", f->name, pk->name, match_id, stake, pay);
    return 0;
}

int predict_restore(Tournament *t, int fan_id, int match_id, int pick, int stake, int payout) {
    if (!fan_by_id(t, fan_id) || match_id < 1 || match_id > t->matchCount || stake < 1) return -1;
    char key[40]; pred_key(key, sizeof key, fan_id, match_id);
    if (hashmap_get(&t->predIndex, key)) return -1;
    store(t, fan_id, match_id, pick, stake, payout < stake ? stake + 1 : payout);
    return 0;
}

PredState predict_state(const Tournament *t, const Prediction *p) {
    const Match *m = t->matches ? t->matches[p->matchId] : NULL;
    if (!m || m->status != M_DONE || !m->winner) return PRED_OPEN;
    return m->winner->id == p->pickId ? PRED_WON : PRED_LOST;
}

int fan_points(const Tournament *t, const Fan *f) {
    int pts = FAN_START_POINTS;
    LIST_FOREACH(n, &t->predictions) {
        const Prediction *p = n->data; if (p->fanId != f->id) continue;
        pts -= p->stake;
        if (predict_state(t, p) == PRED_WON) pts += p->payout;
    }
    return pts;
}
int fan_open_stakes(const Tournament *t, const Fan *f) {
    int s = 0;
    LIST_FOREACH(n, &t->predictions) { const Prediction *p = n->data; if (p->fanId == f->id && predict_state(t, p) == PRED_OPEN) s += p->stake; }
    return s;
}
int fan_won(const Tournament *t, const Fan *f) {
    int c = 0;
    LIST_FOREACH(n, &t->predictions) { const Prediction *p = n->data; if (p->fanId == f->id && predict_state(t, p) == PRED_WON) c++; }
    return c;
}
int fan_total(const Tournament *t, const Fan *f) {
    int c = 0;
    LIST_FOREACH(n, &t->predictions) if (((Prediction *)n->data)->fanId == f->id) c++;
    return c;
}

/* ---- leaderboard: a max-heap needs a comparator that can see the tournament ---- */
static const Tournament *g_rank_t;
static int cmp_fan(const void *x, const void *y) {
    const Fan *a = x, *b = y;
    int pa = fan_points(g_rank_t, a), pb = fan_points(g_rank_t, b);
    if (pa != pb) return pb - pa;
    return a->id - b->id;
}
int fan_leaderboard(Tournament *t, Fan **out, int cap) {
    int prev = trace_enable(0);
    g_rank_t = t;
    Heap h; heap_init(&h, cmp_fan, "fans");
    LIST_FOREACH(n, &t->fans) heap_push(&h, n->data);
    int k = 0; Fan *f;
    while (k < cap && (f = heap_pop(&h))) out[k++] = f;
    heap_free(&h);
    trace_enable(prev);
    return k;
}

void predict_settle_events(Tournament *t, const Match *m) {
    LIST_FOREACH(n, &t->predictions) {
        const Prediction *p = n->data; if (p->matchId != m->id) continue;
        const Fan *f = fan_by_id(t, p->fanId); if (!f) continue;
        if (predict_state(t, p) == PRED_WON) event_log(t, "predict", "%s called match #%d right and wins %d points", f->name, m->id, p->payout);
        else event_log(t, "predict", "%s lost %d points on match #%d", f->name, p->stake, m->id);
    }
}

int predict_prune(Tournament *t) {
    int dropped = 0;
    for (ListNode *n = t->predictions.head; n; ) {
        ListNode *next = n->next; Prediction *p = n->data;
        const Match *m = t->matches ? t->matches[p->matchId] : NULL;
        int bad = !m || (m->status != M_READY && m->status != M_DONE) ||
                  (m->status == M_READY && (!m->a || !m->b || (p->pickId != m->a->id && p->pickId != m->b->id)));
        if (bad) {
            char key[40]; pred_key(key, sizeof key, p->fanId, p->matchId);
            hashmap_remove(&t->predIndex, key);
            list_remove(&t->predictions, n);
            free(p); dropped++;
        }
        n = next;
    }
    return dropped;
}
