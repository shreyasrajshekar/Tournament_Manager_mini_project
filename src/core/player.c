#include "core/player.h"
#include "core/history.h"
#include "core/tournament.h"
#include "util/util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(char *err, size_t cap, const char *msg) { if (err) snprintf(err, cap, "%s", msg); return -1; }

const char *position_name(Position p) { return p == POS_GK ? "GK" : p == POS_DEF ? "DEF" : p == POS_MID ? "MID" : "FWD"; }
int position_parse(const char *s, Position *out) {
    if (!s) return -1;
    static const char *N[] = { "GK", "DEF", "MID", "FWD" };
    char k[8]; str_lower(k, sizeof k, s);
    for (int i = 0; i < 4; i++) { char n[8]; str_lower(n, sizeof n, N[i]); if (!strcmp(k, n)) { *out = (Position)i; return 0; } }
    return -1;
}

Player *player_find(Tournament *t, const char *name) {
    char key[PLAYER_NAME_MAX + 1]; str_lower(key, sizeof key, name);
    return hashmap_get(&t->playerIndex, key);
}
Player *player_by_id(Tournament *t, int id) {
    LIST_FOREACH(n, &t->teams) LIST_FOREACH(p, &((Team *)n->data)->squad) if (((Player *)p->data)->id == id) return p->data;
    return NULL;
}

int player_add(Tournament *t, int team_id, const char *name, Position pos, int number, char *err, size_t ecap) {
    Team *tm = tournament_team_by_id(t, team_id);
    if (!tm) return fail(err, ecap, "Club not found.");
    char buf[PLAYER_NAME_MAX]; snprintf(buf, sizeof buf, "%s", name ? name : "");
    str_sanitize(buf); str_trim(buf);
    if (!*buf) return fail(err, ecap, "Player name cannot be empty.");
    if (tm->squad.size >= MAX_SQUAD) return fail(err, ecap, "A squad can have at most 25 players.");
    if (number < 1 || number > 99) return fail(err, ecap, "Shirt number must be between 1 and 99.");
    char key[PLAYER_NAME_MAX + 1]; str_lower(key, sizeof key, buf);
    if (hashmap_get(&t->playerIndex, key)) return fail(err, ecap, "A player with that name is already registered.");
    LIST_FOREACH(n, &tm->squad) if (((Player *)n->data)->number == number) return fail(err, ecap, "That shirt number is taken in this squad.");
    Player *p = xcalloc(1, sizeof *p);
    p->id = t->nextPlayerId++; snprintf(p->name, sizeof p->name, "%s", buf); p->pos = pos; p->number = number; p->team = tm;
    hashmap_put(&t->playerIndex, key, p);
    list_push_back(&tm->squad, p);
    trace_log("[HASH] index player '%s' -> bucket (players=%zu)", p->name, t->playerIndex.size);
    trace_log("[LIST] append %s #%d to %s squad (size=%zu)", p->name, p->number, tm->name, tm->squad.size);
    event_log(t, "player", "%s (%s, #%d) joined %s", p->name, position_name(pos), number, tm->name);
    return 0;
}

int player_remove(Tournament *t, int id, char *err, size_t ecap) {
    LIST_FOREACH(n, &t->teams) {
        Team *tm = n->data;
        LIST_FOREACH(pn, &tm->squad) {
            Player *p = pn->data;
            if (p->id != id) continue;
            char key[PLAYER_NAME_MAX + 1]; str_lower(key, sizeof key, p->name);
            hashmap_remove(&t->playerIndex, key);
            list_remove(&tm->squad, pn);
            trace_log("[LIST] unlink %s from %s squad in O(1)", p->name, tm->name);
            event_log(t, "player", "%s left %s", p->name, tm->name);
            free(p);
            return 0;
        }
    }
    return fail(err, ecap, "Player not found.");
}

void player_unindex_squad(Tournament *t, Team *tm) {
    LIST_FOREACH(n, &tm->squad) {
        char key[PLAYER_NAME_MAX + 1]; str_lower(key, sizeof key, ((Player *)n->data)->name);
        hashmap_remove(&t->playerIndex, key);
    }
}

static const char *FIRST[] = { "Liam", "Noah", "Mateo", "Luca", "Ravi", "Omar", "Jonas", "Diego", "Kai", "Elias", "Arjun", "Tariq",
    "Felix", "Hugo", "Ivan", "Leo", "Marco", "Nico", "Pablo", "Ruben", "Sami", "Theo", "Yuri", "Zane" };
static const char *LAST[] = { "Silva", "Moreau", "Rossi", "Keller", "Nakamura", "Okafor", "Petrov", "Santos", "Haddad", "Larsen",
    "Costa", "Novak", "Reyes", "Bianchi", "Duarte", "Fischer", "Ibrahim", "Jensen", "Kovac", "Mendes", "Ortega", "Pires", "Quinn", "Varga" };
/* 1 GK, 4 DEF, 4 MID, 2 FWD with classic shirt numbers */
static const struct { Position pos; int number; } SQUAD11[11] = {
    { POS_GK, 1 }, { POS_DEF, 2 }, { POS_DEF, 3 }, { POS_DEF, 4 }, { POS_DEF, 5 },
    { POS_MID, 6 }, { POS_MID, 8 }, { POS_MID, 10 }, { POS_MID, 7 }, { POS_FWD, 9 }, { POS_FWD, 11 } };

int player_fill_squad(Tournament *t, int team_id, char *err, size_t ecap) {
    Team *tm = tournament_team_by_id(t, team_id);
    if (!tm) return fail(err, ecap, "Club not found.");
    int added = 0; char nm[PLAYER_NAME_MAX], e2[100];
    for (int k = 0; k < 11 && tm->squad.size < MAX_SQUAD; k++) {
        int taken = 0; LIST_FOREACH(n, &tm->squad) if (((Player *)n->data)->number == SQUAD11[k].number) taken = 1;
        if (taken) continue;
        for (int tries = 0; tries < 200; tries++) {
            snprintf(nm, sizeof nm, "%s %s", FIRST[rng_next() % 24], LAST[rng_next() % 24]);
            if (!player_add(t, team_id, nm, SQUAD11[k].pos, SQUAD11[k].number, e2, sizeof e2)) { added++; break; }
        }
    }
    if (!added) return fail(err, ecap, "Squad is already full of starters.");
    return 0;
}
