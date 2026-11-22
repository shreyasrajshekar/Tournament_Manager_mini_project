#include "core/persist.h"
#include "core/tournament.h"
#include "core/player.h"
#include "core/predict.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Save format (plain text, one record per line). Loading replays it in order:
 *   TM2 / name / team / player / fan / generate / predict / result
 * Older TM1 saves still load (their court and timing lines are ignored). */
void persist_write(const Tournament *t, StrBuf *o) {
    sb_puts(o, "TM2\n");
    sb_printf(o, "name|%s\n", t->name);
    LIST_FOREACH(n, &t->teams) { const Team *tm = n->data; sb_printf(o, "team|%s|%.1f\n", tm->name, tm->rating0); }
    LIST_FOREACH(n, &t->teams) {
        const Team *tm = n->data;
        LIST_FOREACH(p, &tm->squad) { const Player *pl = p->data; sb_printf(o, "player|%s|%s|%s|%d\n", tm->name, pl->name, position_name(pl->pos), pl->number); }
    }
    LIST_FOREACH(n, &t->fans) sb_printf(o, "fan|%s\n", ((const Fan *)n->data)->name);
    if (t->phase != PHASE_SETUP) sb_puts(o, "generate|1\n");
    LIST_FOREACH(n, &t->predictions) {
        const Prediction *p = n->data; const Fan *f = fan_by_id((Tournament *)t, p->fanId); const Team *pk = tournament_team_by_id((Tournament *)t, p->pickId);
        if (f && pk) sb_printf(o, "predict|%s|%d|%s|%d|%d\n", f->name, p->matchId, pk->name, p->stake, p->payout);
    }
    for (size_t i = 0; i < t->undo.size; i++) {
        const Result *r = t->undo.data[i];
        sb_printf(o, "result|%d|%d|%d\n", r->matchId, r->scoreA, r->scoreB);
    }
}

int persist_save(const Tournament *t, const char *path) {
    StrBuf sb; sb_init(&sb); persist_write(t, &sb);
    FILE *f = fopen(path, "wb");
    if (!f) { sb_free(&sb); return -1; }
    fwrite(sb.data, 1, sb.len, f); fclose(f); sb_free(&sb);
    return 0;
}

static const char *next(void) { return strtok(NULL, "|"); }

/* Copies one line (without the newline) from src into dst; returns the bytes consumed. */
static size_t sscanf_line(const char *src, char *dst, size_t cap) {
    size_t n = 0, used = 0;
    while (src[used] && src[used] != '\n') { if (n + 1 < cap) dst[n++] = src[used]; used++; }
    if (src[used] == '\n') used++;
    dst[n] = 0; return used;
}

Tournament *persist_load_text(const char *text, char *err, size_t ecap) {
    char line[512]; Tournament *t = NULL;
    char name[64] = "Untitled Cup";
    const char *cur = text;
    #define NEXT_LINE() (*cur ? (cur += sscanf_line(cur, line, sizeof line), 1) : 0)
    if (!NEXT_LINE() || (strncmp(line, "TM1", 3) && strncmp(line, "TM2", 3))) { snprintf(err, ecap, "bad save data"); return NULL; }
    char e2[160];
    while (NEXT_LINE()) {
        line[strcspn(line, "\r\n")] = 0;
        char *tag = strtok(line, "|");
        if (!tag) continue;
        if (!strcmp(tag, "name")) { const char *v = next(); if (v) snprintf(name, sizeof name, "%s", v); continue; }
        if (!strcmp(tag, "venues") || !strcmp(tag, "minutes") || !strcmp(tag, "break")) continue;   /* old TM1 timing lines */
        if (!t) t = tournament_create(name);
        if (!strcmp(tag, "team")) { const char *n = next(), *r = next(); if (n) tournament_add_team(t, n, r ? atof(r) : 1500, e2, sizeof e2); }
        else if (!strcmp(tag, "player")) {
            const char *club = next(), *pn = next(), *ps = next(), *num = next(); Position pos = POS_MID;
            Team *tm = club ? tournament_find_team(t, club) : NULL;
            if (tm && pn && !position_parse(ps, &pos)) player_add(t, tm->id, pn, pos, num ? atoi(num) : 0, e2, sizeof e2);
        }
        else if (!strcmp(tag, "fan")) { const char *n = next(); if (n) fan_add(t, n, e2, sizeof e2); }
        else if (!strcmp(tag, "generate")) tournament_generate(t, e2, sizeof e2);
        else if (!strcmp(tag, "predict")) {
            const char *fn = next(), *mid = next(), *pk = next(), *st = next(), *py = next();
            Fan *fan = fn ? fan_find(t, fn) : NULL; Team *tm = pk ? tournament_find_team(t, pk) : NULL;
            if (fan && tm && mid && st) predict_restore(t, fan->id, atoi(mid), tm->id, atoi(st), py ? atoi(py) : 0);
        }
        else if (!strcmp(tag, "result")) {
            int id = atoi(next() ?: "0"), a = atoi(next() ?: "0"), b = atoi(next() ?: "0");
            tournament_play(t, id, a, b, e2, sizeof e2);
        }
    }
    if (!t) t = tournament_create(name);
    return t;
}

Tournament *persist_load(const char *path, char *err, size_t ecap) {
    FILE *f = fopen(path, "rb");
    if (!f) { snprintf(err, ecap, "no save file"); return NULL; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); snprintf(err, ecap, "out of memory"); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f); buf[got] = 0; fclose(f);
    Tournament *t = persist_load_text(buf, err, ecap);
    free(buf); return t;
}
