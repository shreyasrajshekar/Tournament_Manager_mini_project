#include "core/tournament.h"
#include "core/team.h"
#include "core/bracket.h"
#include "core/upnext.h"
#include "core/player.h"
#include "core/predict.h"
#include "core/history.h"
#include "util/util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(char *err, size_t cap, const char *msg) { if (err) snprintf(err, cap, "%s", msg); return -1; }
const char *phase_name(Phase p) { return p == PHASE_SETUP ? "setup" : p == PHASE_RUNNING ? "running" : "finished"; }

Tournament *tournament_create(const char *name) {
    Tournament *t = xcalloc(1, sizeof *t);
    snprintf(t->name, sizeof t->name, "%s", name && *name ? name : "Untitled Cup");
    str_sanitize(t->name);
    t->phase = PHASE_SETUP; t->nextTeamId = 1; t->nextPlayerId = 1; t->nextFanId = 1; t->nextPredId = 1;
    list_init(&t->teams); hashmap_init(&t->teamIndex, 8);
    hashmap_init(&t->playerIndex, 16);
    list_init(&t->fans); hashmap_init(&t->fanIndex, 8);
    list_init(&t->predictions); hashmap_init(&t->predIndex, 16);
    stack_init(&t->undo); list_init(&t->events);
    event_log(t, "create", "Tournament \"%s\" created", t->name);
    return t;
}

void tournament_destroy(Tournament *t) {
    if (!t) return;
    bracket_free(t);
    history_free_all(t); stack_free(&t->undo);
    list_clear(&t->events, event_free);
    list_clear(&t->predictions, free); hashmap_free(&t->predIndex);
    list_clear(&t->fans, free); hashmap_free(&t->fanIndex);
    hashmap_free(&t->playerIndex);
    list_clear(&t->teams, team_free);
    hashmap_free(&t->teamIndex);
    free(t);
}

Team *tournament_find_team(Tournament *t, const char *name) {
    char key[TEAM_NAME_MAX + 1]; str_lower(key, sizeof key, name);
    return hashmap_get(&t->teamIndex, key);
}
Team *tournament_team_by_id(Tournament *t, int id) {
    LIST_FOREACH(n, &t->teams) if (((Team *)n->data)->id == id) return n->data;
    return NULL;
}

int tournament_add_team(Tournament *t, const char *name, double rating, char *err, size_t ecap) {
    if (t->phase != PHASE_SETUP) return fail(err, ecap, "Teams can only be added before the bracket is generated.");
    char buf[TEAM_NAME_MAX]; snprintf(buf, sizeof buf, "%s", name ? name : "");
    str_sanitize(buf); str_trim(buf);
    if (!*buf) return fail(err, ecap, "Team name cannot be empty.");
    if (t->teams.size >= MAX_TEAMS) return fail(err, ecap, "Maximum of 64 teams reached.");
    if (rating < 800 || rating > 3000) rating = 1500;
    char key[TEAM_NAME_MAX + 1]; str_lower(key, sizeof key, buf);
    if (hashmap_get(&t->teamIndex, key)) return fail(err, ecap, "A team with that name is already registered.");
    Team *tm = team_new(t->nextTeamId++, buf, rating);
    hashmap_put(&t->teamIndex, key, tm);
    list_push_back(&t->teams, tm);
    trace_log("[LIST] append team '%s' to registry (size=%zu)", tm->name, t->teams.size);
    event_log(t, "team", "Registered %s (rating %.0f)", tm->name, tm->rating);
    return 0;
}

int tournament_remove_team(Tournament *t, int id, char *err, size_t ecap) {
    if (t->phase != PHASE_SETUP) return fail(err, ecap, "Cannot remove teams after the bracket is generated.");
    LIST_FOREACH(n, &t->teams) {
        Team *tm = n->data;
        if (tm->id != id) continue;
        char key[TEAM_NAME_MAX + 1]; str_lower(key, sizeof key, tm->name);
        hashmap_remove(&t->teamIndex, key);
        player_unindex_squad(t, tm);
        list_remove(&t->teams, n);
        trace_log("[LIST] unlink team '%s' in O(1) (size=%zu)", tm->name, t->teams.size);
        event_log(t, "team", "Removed %s", tm->name);
        team_free(tm);
        return 0;
    }
    return fail(err, ecap, "Team not found.");
}

int tournament_generate(Tournament *t, char *err, size_t ecap) {
    if (t->phase != PHASE_SETUP) return fail(err, ecap, "Bracket already generated.");
    size_t n = t->teams.size;
    if (n < 2) return fail(err, ecap, "Register at least 2 teams.");
    if (n > 32 || (n & (n - 1)) != 0) { if (err) snprintf(err, ecap, "A cup needs 2, 4, 8, 16 or 32 clubs (you have %zu).", n); return -1; }
    bracket_seed_teams(t);
    bracket_build(t);
    t->phase = PHASE_RUNNING;
    event_log(t, "bracket", "Bracket generated: %zu teams, %d-slot tree, %d rounds, %d byes",
              t->teams.size, t->bracketSize, t->rounds, t->bracketSize - (int)t->teams.size);
    return 0;
}

/* ---- applying a result: the heart of "play match -> advance -> update everything" ---- */
static int apply_result(Tournament *t, Match *m, int sa, int sb) {
    Team *A = m->a, *B = m->b;
    int a_won = sa > sb;
    Team *W = a_won ? A : B, *L = a_won ? B : A;

    Result *r = xcalloc(1, sizeof *r);
    r->seq = ++t->resultSeq; r->matchId = m->id; r->round = m->round;
    r->teamA = A->id; r->teamB = B->id; r->winnerId = W->id;
    snprintf(r->nameA, sizeof r->nameA, "%s", A->name); snprintf(r->nameB, sizeof r->nameB, "%s", B->name);
    r->scoreA = sa; r->scoreB = sb;
    r->ratingA_before = A->rating; r->ratingB_before = B->rating;
    r->nextMatchId = m->parent ? m->parent->id : 0;

    m->scoreA = sa; m->scoreB = sb; m->winner = W; m->status = M_DONE;
    A->pointsFor += sa; A->pointsAgainst += sb; B->pointsFor += sb; B->pointsAgainst += sa;
    W->wins++; L->losses++; L->alive = 0;
    elo_update(A, B, a_won);
    r->ratingA_after = A->rating; r->ratingB_after = B->rating;

    list_push_front(&A->history, r); list_push_front(&B->history, r);
    stack_push(&t->undo, r);
    trace_log("[STACK] push result #%d onto undo stack (depth=%zu)", r->seq, stack_size(&t->undo));
    trace_log("[LIST] prepend result to match history of %s and %s", A->name, B->name);
    trace_log("[RANK] Elo: %s %.1f -> %.1f | %s %.1f -> %.1f", A->name, r->ratingA_before, A->rating, B->name, r->ratingB_before, B->rating);

    char rn[32]; bracket_round_name(t, m->round, rn, sizeof rn);
    if (m->parent) {
        event_log(t, "result", "%s: %s %d-%d %s -> %s advances to match #%d", rn, A->name, sa, sb, B->name, W->name, m->parent->id);
        bracket_advance(t, m);
    } else {
        t->champion = W; W->reached = t->rounds + 1; t->phase = PHASE_FINISHED;
        event_log(t, "result", "%s: %s %d-%d %s", rn, A->name, sa, sb, B->name);
        event_log(t, "champion", "%s are the champions of %s!", W->name, t->name);
        trace_log("[TREE] root reached: %s is champion", W->name);
    }
    if (!t->quiet) predict_settle_events(t, m);
    return 0;
}

int tournament_play(Tournament *t, int id, int sa, int sb, char *err, size_t ecap) {
    if (t->phase == PHASE_SETUP) return fail(err, ecap, "Generate the bracket first.");
    if (t->phase == PHASE_FINISHED) return fail(err, ecap, "Tournament is finished.");
    if (id < 1 || id > t->matchCount) return fail(err, ecap, "Unknown match.");
    Match *m = t->matches[id];
    if (m->status != M_READY) return fail(err, ecap, m->status == M_DONE ? "Match already played." : "Match is not ready yet.");
    if (sa < 0 || sb < 0 || sa > 999 || sb > 999) return fail(err, ecap, "Scores must be between 0 and 999.");
    if (sa == sb) return fail(err, ecap, "Knockout matches cannot end in a draw.");
    return apply_result(t, m, sa, sb);
}

static void gen_scores(const Match *m, int *sa, int *sb) {
    double p = elo_expected(m->a->rating, m->b->rating);
    int a_wins = rng_double() < p;
    int w = 1 + (int)(rng_next() % 5), l = (int)(rng_next() % (unsigned)w);
    *sa = a_wins ? w : l; *sb = a_wins ? l : w;
}

int tournament_sim(Tournament *t, int id, char *err, size_t ecap) {
    if (t->phase != PHASE_RUNNING) return fail(err, ecap, t->phase == PHASE_SETUP ? "Generate the bracket first." : "Tournament is finished.");
    if (id == 0) {
        Match *up[1];
        if (upnext_list(t, up, 1) < 1) return fail(err, ecap, "No match is ready.");
        id = up[0]->id;
        trace_log("[HEAP:upcoming] next scheduled match is #%d", id);
    }
    if (id < 1 || id > t->matchCount) return fail(err, ecap, "Unknown match.");
    Match *m = t->matches[id];
    if (m->status != M_READY) return fail(err, ecap, "Match is not ready.");
    int sa, sb; gen_scores(m, &sa, &sb);
    return tournament_play(t, id, sa, sb, err, ecap);
}

int tournament_sim_round(Tournament *t, char *err, size_t ecap) {
    if (t->phase != PHASE_RUNNING) return fail(err, ecap, "Nothing to simulate.");
    int round = 0;
    for (int i = 1; i <= t->matchCount; i++)
        if (t->matches[i]->status == M_READY && (!round || t->matches[i]->round < round)) round = t->matches[i]->round;
    if (!round) return fail(err, ecap, "No match is ready.");
    for (int i = 1; i <= t->matchCount; i++) {
        Match *m = t->matches[i];
        if (m->round == round && m->status == M_READY && tournament_sim(t, i, err, ecap)) return -1;
    }
    return 0;
}

int tournament_sim_all(Tournament *t, char *err, size_t ecap) {
    if (t->phase != PHASE_RUNNING) return fail(err, ecap, "Nothing to simulate.");
    while (t->phase == PHASE_RUNNING) if (tournament_sim(t, 0, err, ecap)) return -1;
    return 0;
}

/* ---- undo = pop the last result, rebuild from seeds, replay the rest ---- */
typedef struct { int id, sa, sb; } Replay;

int tournament_undo(Tournament *t, char *err, size_t ecap) {
    if (stack_size(&t->undo) == 0) return fail(err, ecap, "Nothing to undo.");
    size_t n = stack_size(&t->undo);
    Result *last = stack_peek(&t->undo);
    char msg[160]; snprintf(msg, sizeof msg, "Undid match #%d: %s %d-%d %s", last->matchId, last->nameA, last->scoreA, last->scoreB, last->nameB);
    trace_log("[STACK] pop result #%d (depth %zu -> %zu); replaying remaining %zu results", last->seq, n, n - 1, n - 1);

    Replay *rp = xmalloc(n * sizeof *rp);
    for (size_t i = 0; i + 1 < n; i++) { Result *r = t->undo.data[i]; rp[i].id = r->matchId; rp[i].sa = r->scoreA; rp[i].sb = r->scoreB; }

    int prev = trace_enable(0); t->quiet = 1;
    LIST_FOREACH(tn, &t->teams) team_reset_stats(tn->data);
    history_free_all(t); t->resultSeq = 0;
    bracket_free(t); t->champion = NULL;
    bracket_build(t);
    t->phase = PHASE_RUNNING;
    for (size_t i = 0; i + 1 < n; i++) apply_result(t, t->matches[rp[i].id], rp[i].sa, rp[i].sb);
    t->quiet = 0; trace_enable(prev);
    free(rp);
    event_log(t, "undo", "%s", msg);
    int pr = predict_prune(t);
    if (pr) event_log(t, "predict", "%d prediction%s on matches that are no longer playable %s refunded", pr, pr == 1 ? "" : "s", pr == 1 ? "was" : "were");
    return 0;
}
