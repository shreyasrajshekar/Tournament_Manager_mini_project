/* Unit tests for the tournament engine (Member B). */
#include "core/tournament.h"
#include "core/bracket.h"
#include "core/ranking.h"
#include "core/persist.h"
#include "core/player.h"
#include "core/predict.h"
#include "core/upnext.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static Tournament *make(int n) {
    char err[100], nm[32];
    Tournament *t = tournament_create("Test Cup");
    for (int i = 0; i < n; i++) { sprintf(nm, "Team %d", i + 1); assert(!tournament_add_team(t, nm, 2000 - i * 20, err, sizeof err)); }
    return t;
}

int main(void) {
    char err[100];
    trace_enable(0); rng_seed(7);

    /* registration: duplicates (case-insensitive) rejected, removal works */
    Tournament *t = make(5);
    assert(tournament_add_team(t, "TEAM 1", 1500, err, sizeof err) == -1);
    assert(!tournament_remove_team(t, 5, err, sizeof err) && t->teams.size == 4 && t->teamIndex.size == 4);
    assert(!tournament_add_team(t, "Team 5", 1700, err, sizeof err));
    assert(tournament_play(t, 1, 1, 0, err, sizeof err) == -1);   /* not generated yet */

    /* only 2, 4, 8, 16 or 32 clubs may start a cup */
    assert(t->teams.size == 5 && tournament_generate(t, err, sizeof err) == -1 && t->phase == PHASE_SETUP);
    for (int k = 6; k <= 8; k++) { char nm[16]; snprintf(nm, sizeof nm, "Team %d", k); assert(!tournament_add_team(t, nm, 1500, err, sizeof err)); }
    assert(!tournament_generate(t, err, sizeof err));
    /* generation: 8 teams -> 8-slot tree, 7 matches, no byes */
    assert(t->bracketSize == 8 && t->matchCount == 7 && t->rounds == 3);
    Team *s1 = NULL; LIST_FOREACH(n, &t->teams) if (((Team *)n->data)->seed == 1) s1 = n->data;
    assert(s1 && !strcmp(s1->name, "Team 1") && s1->byes == 0);
    Match *lv[16]; assert(bracket_level_order(t, lv, 16) == 7 && lv[0] == t->root);

    /* play: validation, advancement, undo */
    assert(tournament_play(t, 7, 1, 0, err, sizeof err) == -1);   /* final not ready */
    int ready_id = 0; for (int i = 1; i <= 7; i++) if (t->matches[i]->status == M_READY) { ready_id = i; break; }
    assert(ready_id && tournament_play(t, ready_id, 2, 2, err, sizeof err) == -1); /* draw */
    assert(!tournament_play(t, ready_id, 3, 1, err, sizeof err));
    assert(t->matches[ready_id]->status == M_DONE && stack_size(&t->undo) == 1);
    assert(tournament_play(t, ready_id, 3, 1, err, sizeof err) == -1);
    double r_after = t->matches[ready_id]->winner->rating;
    assert(r_after > t->matches[ready_id]->winner->rating0 - 1e-9);
    assert(!tournament_undo(t, err, sizeof err));
    assert(t->matches[ready_id]->status == M_READY && stack_size(&t->undo) == 0);
    assert(t->matches[ready_id]->winner == NULL);
    assert(tournament_undo(t, err, sizeof err) == -1);

    /* full run: champion, exactly N-1 results... byes excluded => (N-1) matches played */
    assert(!tournament_sim_all(t, err, sizeof err));
    assert(t->phase == PHASE_FINISHED && t->champion && stack_size(&t->undo) == 7);
    Team *out[MAX_TEAMS]; assert(ranking_standings(t, out, MAX_TEAMS) == 8 && out[0] == t->champion);
    int pw = ranking_power(t, out, MAX_TEAMS); for (int i = 1; i < pw; i++) assert(out[i - 1]->rating >= out[i]->rating);

    /* persistence round trip reproduces the same champion and ratings */
    assert(!persist_save(t, "/tmp/tm_test.sav"));
    Tournament *t2 = persist_load("/tmp/tm_test.sav", err, sizeof err);
    assert(t2 && t2->champion && !strcmp(t2->champion->name, t->champion->name) && stack_size(&t2->undo) == 7);
    assert(!tournament_undo(t2, err, sizeof err) && t2->phase == PHASE_RUNNING && !t2->champion);
    tournament_destroy(t2); tournament_destroy(t);

    /* power-of-two sizes: every size 2..32 completes with N-1 played matches */
    for (int n = 2; n <= 32; n++) {
        Tournament *x = make(n);
        if (n & (n - 1)) { assert(tournament_generate(x, err, sizeof err) == -1 && x->phase == PHASE_SETUP); tournament_destroy(x); continue; }
        assert(!tournament_generate(x, err, sizeof err));
        assert(!tournament_sim_all(x, err, sizeof err));
        assert((int)stack_size(&x->undo) == n - 1);
        /* undo everything then redo */
        while (stack_size(&x->undo)) assert(!tournament_undo(x, err, sizeof err));
        assert(x->phase == PHASE_RUNNING && !x->champion);
        assert(!tournament_sim_all(x, err, sizeof err) && x->champion);
        tournament_destroy(x);
    }
    /* ---- squads: hash-map lookup, duplicates, shirt numbers, limits, removal ---- */
    {
        Tournament *x = make(3);
        Team *a = tournament_find_team(x, "team 1"), *b = tournament_find_team(x, "team 2");
        assert(!player_add(x, a->id, "Luca Rossi", POS_FWD, 9, err, sizeof err));
        assert(player_find(x, "LUCA ROSSI") && player_find(x, "luca rossi")->team == a);
        assert(player_add(x, b->id, "luca rossi", POS_MID, 9, err, sizeof err) == -1);      /* duplicate name */
        assert(player_add(x, a->id, "Omar Haddad", POS_DEF, 9, err, sizeof err) == -1);     /* shirt number taken */
        assert(player_add(x, a->id, "Omar Haddad", POS_DEF, 0, err, sizeof err) == -1);     /* bad number */
        assert(player_add(x, 999, "Ghost", POS_GK, 1, err, sizeof err) == -1);              /* unknown club */
        assert(!player_add(x, b->id, "Omar Haddad", POS_DEF, 9, err, sizeof err));          /* same number, other club */
        assert(x->playerIndex.size == 2 && a->squad.size == 1);
        Player *lr = player_find(x, "Luca Rossi");
        assert(!player_remove(x, lr->id, err, sizeof err) && !player_find(x, "Luca Rossi") && a->squad.size == 0);
        assert(player_remove(x, 12345, err, sizeof err) == -1);
        for (int i = 0; i < 25; i++) { char nm[32]; sprintf(nm, "Filler %d", i); assert(!player_add(x, a->id, nm, POS_MID, i + 1, err, sizeof err)); }
        assert(player_add(x, a->id, "One Too Many", POS_MID, 30, err, sizeof err) == -1);   /* squad limit */
        assert(!player_fill_squad(x, b->id, err, sizeof err) && b->squad.size >= 11);
        size_t before = x->playerIndex.size;
        assert(!tournament_remove_team(x, a->id, err, sizeof err) && x->playerIndex.size == before - 25);  /* squad leaves the index */
        tournament_destroy(x);
    }

    /* ---- predictions: virtual points, settlement, undo, persistence ---- */
    {
        Tournament *x = make(4);
        assert(fan_add(x, "Aarav", err, sizeof err) == 0 && fan_add(x, "maya", err, sizeof err) == 0);
        assert(fan_add(x, "AARAV", err, sizeof err) == -1 && fan_add(x, "  ", err, sizeof err) == -1);
        Fan *aarav = fan_find(x, "aarav"), *maya = fan_find(x, "MAYA");
        assert(fan_points(x, aarav) == FAN_START_POINTS);
        assert(predict_place(x, aarav->id, 1, 1, 10, err, sizeof err) == -1);              /* not running yet */
        assert(!tournament_generate(x, err, sizeof err));
        Match *up[8]; int nu = upnext_list(x, up, 8); assert(nu == 2);
        Match *m = up[0]; int fav = m->a->rating > m->b->rating ? m->a->id : m->b->id, dog = fav == m->a->id ? m->b->id : m->a->id;
        assert(predict_place(x, aarav->id, m->id, 99, 10, err, sizeof err) == -1);         /* pick not playing */
        assert(predict_place(x, aarav->id, m->id, fav, 0, err, sizeof err) == -1);         /* zero stake */
        assert(predict_place(x, aarav->id, m->id, fav, 101, err, sizeof err) == -1);       /* more than the balance */
        assert(predict_place(x, aarav->id, 3, fav, 10, err, sizeof err) == -1);            /* final has no clubs yet */
        assert(!predict_place(x, aarav->id, m->id, fav, 40, err, sizeof err));
        assert(predict_place(x, aarav->id, m->id, dog, 10, err, sizeof err) == -1);        /* one per fan per match */
        assert(!predict_place(x, maya->id, m->id, dog, 40, err, sizeof err));
        assert(fan_points(x, aarav) == 60 && fan_open_stakes(x, aarav) == 40);             /* stake is held */
        int payF = predict_payout_for(x, m, fav, 40), payD = predict_payout_for(x, m, dog, 40);
        assert(payF > 40 && payD >= payF);                                                 /* underdogs pay at least as much */
        tournament_destroy(x);
    }
    {
        Tournament *x = make(4);
        char e[100]; fan_add(x, "Aarav", e, sizeof e); fan_add(x, "Maya", e, sizeof e);
        tournament_generate(x, e, sizeof e);
        Match *up[8]; upnext_list(x, up, 8); Match *m = up[0];
        int aId = m->a->id, bId = m->b->id, mid = m->id;
        assert(!predict_place(x, 1, mid, aId, 40, e, sizeof e) && !predict_place(x, 2, mid, bId, 25, e, sizeof e));
        int payA = x->predictions.head ? ((Prediction *)x->predictions.head->data)->payout : 0;
        assert(!tournament_play(x, mid, 3, 1, e, sizeof e));                       /* club A wins */
        Fan *f1 = fan_by_id(x, 1), *f2 = fan_by_id(x, 2);
        assert(fan_points(x, f1) == 100 - 40 + payA && fan_won(x, f1) == 1 && fan_open_stakes(x, f1) == 0);
        assert(fan_points(x, f2) == 75 && fan_won(x, f2) == 0);
        Fan *lb[8]; assert(fan_leaderboard(x, lb, 8) == 2 && lb[0] == f1);           /* heap leaderboard: best first */
        /* undo takes the points back: both stakes are open again */
        assert(!tournament_undo(x, e, sizeof e));
        assert(fan_points(x, f1) == 60 && fan_points(x, f2) == 75 && fan_open_stakes(x, f1) == 40);
        assert(!tournament_play(x, mid, 0, 2, e, sizeof e));                        /* club B wins this time */
        assert(fan_points(x, f1) == 60 && fan_points(x, f2) > 75);

        /* persistence keeps players, fans and predictions */
        Team *tm = tournament_find_team(x, "Team 1");
        assert(!player_add(x, tm->id, "Kai Larsen", POS_GK, 1, e, sizeof e));
        assert(!persist_save(x, "/tmp/tm_test2.sav"));
        Tournament *y = persist_load("/tmp/tm_test2.sav", e, sizeof e);
        assert(y && y->fans.size == 2 && y->predictions.size == 2 && player_find(y, "kai larsen"));
        assert(fan_points(y, fan_find(y, "Aarav")) == fan_points(x, f1) && fan_points(y, fan_find(y, "Maya")) == fan_points(x, f2));
        assert(stack_size(&y->undo) == 1);
        tournament_destroy(y);
        tournament_destroy(x);
    }
    {   /* a prediction on a later match is dropped when its feeder result is undone */
        Tournament *x = make(4); char e[100];
        fan_add(x, "Aarav", e, sizeof e); tournament_generate(x, e, sizeof e);
        Match *up[8]; upnext_list(x, up, 8);
        assert(!tournament_play(x, up[0]->id, 2, 1, e, sizeof e) && !tournament_play(x, up[1]->id, 2, 1, e, sizeof e));
        assert(x->matches[3]->status == M_READY);
        assert(!predict_place(x, 1, 3, x->matches[3]->a->id, 30, e, sizeof e) && fan_points(x, fan_by_id(x, 1)) == 70);
        assert(!tournament_undo(x, e, sizeof e));                                    /* final becomes pending again */
        assert(x->predictions.size == 0 && fan_points(x, fan_by_id(x, 1)) == 100);
        tournament_destroy(x);
    }

    puts("test_core: all passed");
    return 0;
}
