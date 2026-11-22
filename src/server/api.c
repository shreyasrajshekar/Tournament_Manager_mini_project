#include "server/api.h"
#include "server/json.h"
#include "core/tournament.h"
#include "core/bracket.h"
#include "core/ranking.h"
#include "core/upnext.h"
#include "core/player.h"
#include "core/predict.h"
#include "core/persist.h"
#include "core/db.h"
#include "server/report.h"
#include "core/team.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static Tournament *g_t = NULL;
static char g_save[400] = "data/tournament.db";

static int use_db(void) { size_t n = strlen(g_save); return !(n > 3 && !strcmp(g_save + n - 3, ".tm")); }

static const char *SAMPLE[] = {
    "Neon Falcons FC", "Iron Wolves FC", "Solar Drakes FC", "Midnight Owls FC", "Crimson Vipers FC", "Arctic Foxes FC", "Thunder Hawks FC",
    "Jade Dragons FC", "Silver Sharks FC", "Golden Lynx FC", "Storm Riders FC", "Shadow Pumas FC", "Cobalt Knights FC", "Ember Bears FC",
    "Titan Rhinos FC", "Echo Ravens FC", "Lunar Stags FC", "Blaze Panthers FC", "Velvet Cobras FC", "Frost Giants FC", "Rogue Comets FC",
    "Atlas Bulls FC", "Zephyr Gulls FC", "Onyx Badgers FC", "Quartz Otters FC", "Radiant Elks FC", "Mystic Moose FC", "Pixel Pirates FC",
    "Turbo Tigers FC", "Nova Narwhals FC", "Gale Gryphons FC", "Orbit Owls FC" };
static const char *SAMPLE_FANS[] = { "Aarav", "Maya", "Diego", "Sofia" };
#define NSAMPLE ((int)(sizeof SAMPLE / sizeof SAMPLE[0]))

void api_init(const char *save_path) {
    if (save_path) snprintf(g_save, sizeof g_save, "%s", save_path);
    char err[100];
    g_t = use_db() ? db_load(g_save, err, sizeof err) : persist_load(g_save, err, sizeof err);
    if (!g_t && use_db()) {                      /* first run on the database: import the old text save once */
        g_t = persist_load("data/autosave.tm", err, sizeof err);
        if (g_t) { db_save(g_t, g_save); rename("data/autosave.tm", "data/autosave.tm.imported"); printf("Imported data/autosave.tm into %s\n", g_save); }
    }
    if (g_t) trace_clear();
}

/* ---------- JSON state serialisation ---------- */
static void j_team_ref(StrBuf *o, const Team *t) {
    if (!t) { sb_puts(o, "null"); return; }
    sb_printf(o, "{\"id\":%d,\"seed\":%d,\"rating\":%.1f,\"name\":", t->id, t->seed, t->rating);
    sb_json_str(o, t->name); sb_puts(o, "}");
}
static void j_team_full(StrBuf *o, const Team *t) {
    sb_printf(o, "{\"id\":%d,\"seed\":%d,\"rating\":%.1f,\"rating0\":%.1f,\"wins\":%d,\"losses\":%d,\"byes\":%d,"
                 "\"pf\":%d,\"pa\":%d,\"reached\":%d,\"alive\":%d,\"squadSize\":%zu,\"name\":",
              t->id, t->seed, t->rating, t->rating0, t->wins, t->losses, t->byes, t->pointsFor, t->pointsAgainst, t->reached, t->alive, t->squad.size);
    sb_json_str(o, t->name); sb_puts(o, "}");
}
static void j_squad(StrBuf *o, const Team *t) {
    sb_puts(o, "[");
    LIST_FOREACH(n, &t->squad) {
        const Player *p = n->data;
        if (n != t->squad.head) sb_puts(o, ",");
        sb_printf(o, "{\"id\":%d,\"pos\":\"%s\",\"num\":%d,\"name\":", p->id, position_name(p->pos), p->number);
        sb_json_str(o, p->name); sb_puts(o, "}");
    }
    sb_puts(o, "]");
}
static const char *status_name(MatchStatus s) { return s == M_PENDING ? "pending" : s == M_READY ? "ready" : s == M_DONE ? "done" : "bye"; }

static void build_state(StrBuf *o) {
    int prev = trace_enable(0);
    Tournament *t = g_t;
    sb_puts(o, "{\"ok\":true,");
    if (!t) { sb_puts(o, "\"exists\":false}"); trace_enable(prev); return; }

    sb_printf(o, "\"exists\":true,\"phase\":\"%s\",\"name\":", phase_name(t->phase)); sb_json_str(o, t->name);
    sb_printf(o, ",\"undoDepth\":%zu,\"rounds\":%d,\"bracketSize\":%d", stack_size(&t->undo), t->rounds, t->bracketSize);

    /* teams (linked list order) */
    sb_puts(o, ",\"teams\":[");
    LIST_FOREACH(n, &t->teams) { if (n != t->teams.head) sb_puts(o, ","); j_team_full(o, n->data); }
    sb_puts(o, "]");

    /* squads: club id -> players (linked list per club) */
    sb_puts(o, ",\"squads\":{");
    LIST_FOREACH(n, &t->teams) { const Team *tm = n->data; if (n != t->teams.head) sb_puts(o, ","); sb_printf(o, "\"%d\":", tm->id); j_squad(o, tm); }
    sb_printf(o, "},\"players\":%zu", t->playerIndex.size);

    /* round names */
    sb_puts(o, ",\"roundNames\":[");
    for (int r = 1; r <= t->rounds; r++) { char b[32]; if (r > 1) sb_puts(o, ","); sb_json_str(o, bracket_round_name(t, r, b, sizeof b)); }
    sb_puts(o, "]");

    /* bracket: level-order traversal (queue-based) */
    sb_puts(o, ",\"matches\":[");
    Match *ms[256]; int nm = bracket_level_order(t, ms, 256);
    for (int i = 0; i < nm; i++) {
        Match *m = ms[i];
        if (i) sb_puts(o, ",");
        sb_printf(o, "{\"id\":%d,\"round\":%d,\"pos\":%d,\"left\":%d,\"right\":%d,\"parent\":%d,\"status\":\"%s\","
                     "\"scoreA\":%d,\"scoreB\":%d,\"winner\":%d,\"a\":",
                  m->id, m->round, m->pos, m->left ? m->left->id : 0, m->right ? m->right->id : 0, m->parent ? m->parent->id : 0,
                  status_name(m->status), m->scoreA, m->scoreB, m->winner ? m->winner->id : 0);
        j_team_ref(o, m->a); sb_puts(o, ",\"b\":"); j_team_ref(o, m->b); sb_puts(o, "}");
    }
    sb_puts(o, "]");

    /* up next (heap ordered by round, then position) */
    sb_puts(o, ",\"upcoming\":[");
    Match *up[64]; int nu = upnext_list(t, up, 64);
    for (int i = 0; i < nu; i++) sb_printf(o, "%s%d", i ? "," : "", up[i]->id);
    sb_puts(o, "]");

    /* rankings */
    Team *rk[MAX_TEAMS]; int k;
    sb_puts(o, ",\"standings\":["); k = ranking_standings(t, rk, MAX_TEAMS);
    for (int i = 0; i < k; i++) { if (i) sb_puts(o, ","); j_team_full(o, rk[i]); }
    sb_puts(o, "],\"power\":["); k = ranking_power(t, rk, MAX_TEAMS);
    for (int i = 0; i < k; i++) { if (i) sb_puts(o, ","); j_team_full(o, rk[i]); }
    sb_puts(o, "]");
    sb_puts(o, ",\"champion\":"); j_team_ref(o, t->champion);

    /* fans (leaderboard = max-heap on virtual points) and predictions (linked list) */
    sb_puts(o, ",\"fans\":[");
    Fan *fl[MAX_FANS]; int nf = fan_leaderboard(t, fl, MAX_FANS);
    for (int i = 0; i < nf; i++) {
        const Fan *f = fl[i];
        sb_printf(o, "%s{\"id\":%d,\"points\":%d,\"open\":%d,\"won\":%d,\"total\":%d,\"name\":", i ? "," : "", f->id,
                  fan_points(t, f), fan_open_stakes(t, f), fan_won(t, f), fan_total(t, f));
        sb_json_str(o, f->name); sb_puts(o, "}");
    }
    sb_puts(o, "],\"predictions\":[");
    for (ListNode *n = t->predictions.tail; n; n = n->prev) {
        const Prediction *p = n->data; const Fan *f = fan_by_id(t, p->fanId); const Team *pk = tournament_team_by_id(t, p->pickId);
        if (!f || !pk) continue;
        PredState ps = predict_state(t, p);
        if (n != t->predictions.tail) sb_puts(o, ",");
        sb_printf(o, "{\"id\":%d,\"fanId\":%d,\"matchId\":%d,\"pickId\":%d,\"stake\":%d,\"payout\":%d,\"state\":\"%s\",\"fan\":",
                  p->id, p->fanId, p->matchId, p->pickId, p->stake, p->payout, ps == PRED_OPEN ? "open" : ps == PRED_WON ? "won" : "lost");
        sb_json_str(o, f->name); sb_puts(o, ",\"pick\":"); sb_json_str(o, pk->name); sb_puts(o, "}");
    }
    sb_puts(o, "]");

    /* match history: undo stack, newest first */
    sb_puts(o, ",\"history\":[");
    for (size_t i = t->undo.size; i > 0; i--) {
        const Result *r = t->undo.data[i - 1];
        if (i != t->undo.size) sb_puts(o, ",");
        sb_printf(o, "{\"seq\":%d,\"matchId\":%d,\"round\":%d,\"next\":%d,\"scoreA\":%d,\"scoreB\":%d,\"winnerId\":%d,"
                     "\"teamA\":%d,\"teamB\":%d,\"ra0\":%.1f,\"ra1\":%.1f,\"rb0\":%.1f,\"rb1\":%.1f,\"nameA\":",
                  r->seq, r->matchId, r->round, r->nextMatchId, r->scoreA, r->scoreB, r->winnerId, r->teamA, r->teamB,
                  r->ratingA_before, r->ratingA_after, r->ratingB_before, r->ratingB_after);
        sb_json_str(o, r->nameA); sb_puts(o, ",\"nameB\":"); sb_json_str(o, r->nameB); sb_puts(o, "}");
    }
    sb_puts(o, "]");

    /* event feed (linked list), newest first, max 80 */
    sb_puts(o, ",\"events\":[");
    int cnt = 0;
    for (ListNode *n = t->events.tail; n && cnt < 80; n = n->prev, cnt++) {
        const Event *e = n->data;
        if (cnt) sb_puts(o, ",");
        sb_printf(o, "{\"seq\":%d,\"kind\":\"%s\",\"msg\":", e->seq, e->kind); sb_json_str(o, e->msg); sb_puts(o, "}");
    }
    sb_puts(o, "]");

    /* data-structure inspector */
    sb_printf(o, ",\"ds\":{\"hash\":{\"buckets\":%zu,\"size\":%zu,\"collisions\":%zu,\"maxChain\":%zu,\"lens\":[",
              t->teamIndex.nbuckets, t->teamIndex.size, t->teamIndex.collisions, hashmap_max_chain(&t->teamIndex));
    for (size_t i = 0; i < t->teamIndex.nbuckets; i++) sb_printf(o, "%s%zu", i ? "," : "", hashmap_bucket_len(&t->teamIndex, i));
    sb_puts(o, "]}");
    sb_printf(o, ",\"tree\":{\"nodes\":%d,\"height\":%d,\"slots\":%d,\"byes\":%d}", t->matchCount, t->rounds, t->bracketSize,
              t->bracketSize ? t->bracketSize - (int)t->teams.size : 0);
    sb_printf(o, ",\"playerHash\":{\"buckets\":%zu,\"size\":%zu,\"collisions\":%zu,\"maxChain\":%zu}",
              t->playerIndex.nbuckets, t->playerIndex.size, t->playerIndex.collisions, hashmap_max_chain(&t->playerIndex));
    sb_puts(o, ",\"rankHeap\":[");
    {   /* internal array layout of a rating max-heap, to visualise the heap property */
        Heap h; heap_init(&h, ranking_cmp_power, "viz");
        LIST_FOREACH(n, &t->teams) heap_push(&h, n->data);
        for (size_t i = 0; i < h.size; i++) { const Team *tm = h.a[i]; if (i) sb_puts(o, ","); sb_printf(o, "{\"rating\":%.0f,\"name\":", tm->rating); sb_json_str(o, tm->name); sb_puts(o, "}"); }
        heap_free(&h);
    }
    sb_printf(o, "],\"lists\":{\"teams\":%zu,\"events\":%zu,\"fans\":%zu,\"predictions\":%zu},\"stack\":%zu}",
              t->teams.size, t->events.size, t->fans.size, t->predictions.size, stack_size(&t->undo));

    /* trace of the last mutating operation */
    sb_puts(o, ",\"trace\":[");
    for (int i = 0; i < trace_count(); i++) { if (i) sb_puts(o, ","); sb_json_str(o, trace_at(i)); }
    sb_puts(o, "]}");
    trace_enable(prev);
}

static void send_state(HttpResponse *res) { build_state(&res->body); }
static void send_error(HttpResponse *res, const char *msg) {
    res->status = 400; res->body.len = 0; res->body.data[0] = 0;
    sb_puts(&res->body, "{\"ok\":false,\"error\":"); sb_json_str(&res->body, msg); sb_puts(&res->body, "}");
}
static void autosave(void) {
    if (!use_db()) { if (g_t) persist_save(g_t, g_save); else remove(g_save); return; }
    if (g_t) db_save(g_t, g_save); else remove(g_save);
}

/* adds `count` sample teams that are not registered yet */
static int fill_sample(Tournament *t, int count) {
    char err[100]; int added = 0;
    for (int i = 0; i < NSAMPLE && added < count && t->teams.size < MAX_TEAMS; i++) {
        if (tournament_find_team(t, SAMPLE[i])) continue;
        if (!tournament_add_team(t, SAMPLE[i], 1300 + (double)(rng_next() % 520), err, sizeof err)) added++;
    }
    return added;
}

/* a sample 11-man squad for every club that has no players yet */
static void fill_squads(Tournament *t) {
    char err[100];
    LIST_FOREACH(n, &t->teams) { Team *tm = n->data; if (tm->squad.size == 0) player_fill_squad(t, tm->id, err, sizeof err); }
}
static int fill_fans(Tournament *t) {
    char err[100]; int added = 0;
    for (int i = 0; i < (int)(sizeof SAMPLE_FANS / sizeof SAMPLE_FANS[0]); i++) if (!fan_find(t, SAMPLE_FANS[i]) && !fan_add(t, SAMPLE_FANS[i], err, sizeof err)) added++;
    return added;
}

void api_handle(const HttpRequest *req, HttpResponse *res) {
    const char *p = req->path;
    int post = strcmp(req->method, "POST") == 0;
    char err[200] = "";
    JVal *body = NULL;
    if (post && req->body_len > 0) body = json_parse(req->body);

    if (!strcmp(p, "/api/state")) { send_state(res); json_free(body); return; }
    if (!strcmp(p, "/api/export")) {
        res->ctype = "text/plain; charset=utf-8";
        res->extra_headers = "Content-Disposition: attachment; filename=\"tournament.tm\"\r\n";
        if (g_t) persist_write(g_t, &res->body);
        json_free(body); return;
    }
    if (!strcmp(p, "/api/report")) {
        res->ctype = "text/html; charset=utf-8";
        res->extra_headers = "Content-Disposition: attachment; filename=\"tournament-report.html\"\r\n";
        if (g_t) report_html(g_t, &res->body);
        json_free(body); return;
    }
    if (!post) { res->status = 404; sb_puts(&res->body, "{\"ok\":false,\"error\":\"not found\"}"); return; }

    trace_clear();
    int rc = 0;
    if (!strcmp(p, "/api/create")) {
        tournament_destroy(g_t);
        rng_seed((unsigned)time(NULL) ^ 0x9e3779b9u);
        g_t = tournament_create(json_str(body, "name", "Untitled Cup"));
    } else if (!strcmp(p, "/api/demo")) {
        int n = (int)json_num(body, "teams", 8); if (n < 2) n = 2; if (n > 32) n = 32;
        while (n & (n - 1)) n &= n - 1;                 /* round down to a power of two */
        tournament_destroy(g_t);
        g_t = tournament_create("Demo Cup");
        fill_sample(g_t, n);
        fill_squads(g_t);
        fill_fans(g_t);
        rc = tournament_generate(g_t, err, sizeof err);
    } else if (!g_t) {
        snprintf(err, sizeof err, "No tournament yet."); rc = -1;
    } else if (!strcmp(p, "/api/team")) {
        rc = tournament_add_team(g_t, json_str(body, "name", ""), json_num(body, "rating", 1500), err, sizeof err);
    } else if (!strcmp(p, "/api/team/remove")) {
        rc = tournament_remove_team(g_t, (int)json_num(body, "id", 0), err, sizeof err);
    } else if (!strcmp(p, "/api/fill")) {
        if (g_t->phase != PHASE_SETUP) { snprintf(err, sizeof err, "Bracket already generated."); rc = -1; }
        else if (fill_sample(g_t, (int)json_num(body, "count", 8)) == 0) { snprintf(err, sizeof err, "No more sample teams available."); rc = -1; }
    } else if (!strcmp(p, "/api/player")) {
        Position pos = POS_MID;
        if (position_parse(json_str(body, "pos", "MID"), &pos)) { snprintf(err, sizeof err, "Position must be GK, DEF, MID or FWD."); rc = -1; }
        else rc = player_add(g_t, (int)json_num(body, "teamId", 0), json_str(body, "name", ""), pos, (int)json_num(body, "number", 0), err, sizeof err);
    } else if (!strcmp(p, "/api/player/remove")) {
        rc = player_remove(g_t, (int)json_num(body, "id", 0), err, sizeof err);
    } else if (!strcmp(p, "/api/squad/fill")) {
        int tid = (int)json_num(body, "teamId", 0);
        if (tid) rc = player_fill_squad(g_t, tid, err, sizeof err); else fill_squads(g_t);
    } else if (!strcmp(p, "/api/fan")) {
        rc = fan_add(g_t, json_str(body, "name", ""), err, sizeof err);
    } else if (!strcmp(p, "/api/fan/fill")) {
        if (fill_fans(g_t) == 0) { snprintf(err, sizeof err, "Sample fans are already added."); rc = -1; }
    } else if (!strcmp(p, "/api/predict")) {
        rc = predict_place(g_t, (int)json_num(body, "fanId", 0), (int)json_num(body, "matchId", 0), (int)json_num(body, "pickId", 0), (int)json_num(body, "stake", 0), err, sizeof err);
    } else if (!strcmp(p, "/api/generate")) {
        rc = tournament_generate(g_t, err, sizeof err);
    } else if (!strcmp(p, "/api/play")) {
        rc = tournament_play(g_t, (int)json_num(body, "matchId", 0), (int)json_num(body, "scoreA", -1), (int)json_num(body, "scoreB", -1), err, sizeof err);
    } else if (!strcmp(p, "/api/sim")) {
        rc = tournament_sim(g_t, (int)json_num(body, "matchId", 0), err, sizeof err);
    } else if (!strcmp(p, "/api/simround")) {
        rc = tournament_sim_round(g_t, err, sizeof err);
    } else if (!strcmp(p, "/api/simall")) {
        rc = tournament_sim_all(g_t, err, sizeof err);
    } else if (!strcmp(p, "/api/undo")) {
        rc = tournament_undo(g_t, err, sizeof err);
    } else if (!strcmp(p, "/api/reset")) {
        tournament_destroy(g_t); g_t = NULL;
    } else {
        snprintf(err, sizeof err, "Unknown endpoint."); rc = -1; res->status = 404;
    }
    json_free(body);
    if (rc) { int st = res->status; send_error(res, err); if (st == 404) res->status = 404; return; }
    autosave();
    send_state(res);
}
