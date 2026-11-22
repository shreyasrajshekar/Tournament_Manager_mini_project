#include "core/db.h"
#include "core/persist.h"
#include "core/ranking.h"
#include "core/predict.h"
#include "core/bracket.h"
#include "core/tournament.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *SCHEMA =
    "CREATE TABLE IF NOT EXISTS log(seq INTEGER PRIMARY KEY, line TEXT NOT NULL);"
    "CREATE TABLE IF NOT EXISTS tournament(id INTEGER PRIMARY KEY CHECK(id=1), name TEXT, phase TEXT, champion TEXT, saved_at TEXT);"
    "CREATE TABLE IF NOT EXISTS teams(id INTEGER PRIMARY KEY, name TEXT, seed INTEGER, strength REAL, elo REAL, wins INTEGER, losses INTEGER, points_for INTEGER, points_against INTEGER);"
    "CREATE TABLE IF NOT EXISTS results(seq INTEGER PRIMARY KEY, match_id INTEGER, round TEXT, team_a TEXT, team_b TEXT, score_a INTEGER, score_b INTEGER, winner TEXT);"
    "CREATE TABLE IF NOT EXISTS fans(id INTEGER PRIMARY KEY, name TEXT, points INTEGER, predictions INTEGER);";

static int run(sqlite3 *db, const char *sql) { return sqlite3_exec(db, sql, NULL, NULL, NULL); }

int db_save(const Tournament *t, const char *path) {
    sqlite3 *db = NULL;
    if (sqlite3_open(path, &db) != SQLITE_OK) { sqlite3_close(db); return -1; }
    int rc = -1;
    if (run(db, SCHEMA) || run(db, "BEGIN IMMEDIATE")) goto done;
    run(db, "DELETE FROM log; DELETE FROM tournament; DELETE FROM teams; DELETE FROM results; DELETE FROM fans;");

    StrBuf sb; sb_init(&sb); persist_write(t, &sb);
    sqlite3_stmt *st = NULL;
    sqlite3_prepare_v2(db, "INSERT INTO log(seq,line) VALUES(?,?)", -1, &st, NULL);
    int seq = 0; char *p = sb.data;
    while (p && *p) {
        char *nl = strchr(p, '\n'); size_t n = nl ? (size_t)(nl - p) : strlen(p);
        sqlite3_bind_int(st, 1, ++seq); sqlite3_bind_text(st, 2, p, (int)n, SQLITE_TRANSIENT);
        sqlite3_step(st); sqlite3_reset(st);
        p = nl ? nl + 1 : NULL;
    }
    sqlite3_finalize(st); sb_free(&sb);

    static const char *PH[] = { "setup", "running", "finished" };
    st = NULL; sqlite3_prepare_v2(db, "INSERT INTO tournament VALUES(1,?,?,?,datetime('now'))", -1, &st, NULL);
    sqlite3_bind_text(st, 1, t->name, -1, SQLITE_TRANSIENT); sqlite3_bind_text(st, 2, PH[t->phase], -1, SQLITE_STATIC);
    if (t->champion) sqlite3_bind_text(st, 3, t->champion->name, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(st, 3);
    sqlite3_step(st); sqlite3_finalize(st);

    st = NULL; sqlite3_prepare_v2(db, "INSERT INTO teams VALUES(?,?,?,?,?,?,?,?,?)", -1, &st, NULL);
    LIST_FOREACH(n, &t->teams) {
        const Team *tm = n->data;
        sqlite3_bind_int(st, 1, tm->id); sqlite3_bind_text(st, 2, tm->name, -1, SQLITE_TRANSIENT); sqlite3_bind_int(st, 3, tm->seed);
        sqlite3_bind_double(st, 4, tm->rating0); sqlite3_bind_double(st, 5, tm->rating); sqlite3_bind_int(st, 6, tm->wins);
        sqlite3_bind_int(st, 7, tm->losses); sqlite3_bind_int(st, 8, tm->pointsFor); sqlite3_bind_int(st, 9, tm->pointsAgainst);
        sqlite3_step(st); sqlite3_reset(st);
    }
    sqlite3_finalize(st);

    st = NULL; sqlite3_prepare_v2(db, "INSERT INTO results VALUES(?,?,?,?,?,?,?,?)", -1, &st, NULL);
    for (size_t i = 0; i < t->undo.size; i++) {
        const Result *r = t->undo.data[i]; char rn[32]; bracket_round_name(t, r->round, rn, sizeof rn);
        sqlite3_bind_int(st, 1, r->seq); sqlite3_bind_int(st, 2, r->matchId); sqlite3_bind_text(st, 3, rn, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 4, r->nameA, -1, SQLITE_TRANSIENT); sqlite3_bind_text(st, 5, r->nameB, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(st, 6, r->scoreA); sqlite3_bind_int(st, 7, r->scoreB);
        sqlite3_bind_text(st, 8, r->winnerId == r->teamA ? r->nameA : r->nameB, -1, SQLITE_TRANSIENT);
        sqlite3_step(st); sqlite3_reset(st);
    }
    sqlite3_finalize(st);

    st = NULL; sqlite3_prepare_v2(db, "INSERT INTO fans VALUES(?,?,?,?)", -1, &st, NULL);
    LIST_FOREACH(n, &t->fans) {
        const Fan *f = n->data;
        sqlite3_bind_int(st, 1, f->id); sqlite3_bind_text(st, 2, f->name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(st, 3, fan_points(t, f)); sqlite3_bind_int(st, 4, fan_total(t, f));
        sqlite3_step(st); sqlite3_reset(st);
    }
    sqlite3_finalize(st);
    rc = run(db, "COMMIT") == SQLITE_OK ? 0 : -1;
done:
    if (rc) run(db, "ROLLBACK");
    sqlite3_close(db);
    return rc;
}

Tournament *db_load(const char *path, char *err, size_t ecap) {
    FILE *probe = fopen(path, "rb");
    if (!probe) { snprintf(err, ecap, "no database yet"); return NULL; }
    fclose(probe);
    sqlite3 *db = NULL;
    if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) { snprintf(err, ecap, "cannot open database"); sqlite3_close(db); return NULL; }
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db, "SELECT line FROM log ORDER BY seq", -1, &st, NULL) != SQLITE_OK) { snprintf(err, ecap, "database has no data"); sqlite3_close(db); return NULL; }
    StrBuf sb; sb_init(&sb);
    while (sqlite3_step(st) == SQLITE_ROW) { sb_puts(&sb, (const char *)sqlite3_column_text(st, 0)); sb_puts(&sb, "\n"); }
    sqlite3_finalize(st); sqlite3_close(db);
    Tournament *t = sb.len ? persist_load_text(sb.data, err, ecap) : NULL;
    if (!sb.len) snprintf(err, ecap, "database is empty");
    sb_free(&sb);
    return t;
}
