/* cli.c - terminal demo of the full flow, no browser needed.
 *   clubs -> squads -> bracket -> predictions -> play -> undo -> standings
 * Usage: tm_cli [clubs=8] [seed=1] */
#include "core/tournament.h"
#include "core/bracket.h"
#include "core/ranking.h"
#include "core/player.h"
#include "core/predict.h"
#include "core/upnext.h"
#include "util/util.h"
#include <stdio.h>
#include <stdlib.h>

static const char *NAMES[] = { "Neon Falcons FC", "Iron Wolves FC", "Solar Drakes FC", "Midnight Owls FC", "Crimson Vipers FC",
    "Arctic Foxes FC", "Thunder Hawks FC", "Jade Dragons FC", "Silver Sharks FC", "Golden Lynx FC", "Storm Riders FC", "Shadow Pumas FC",
    "Cobalt Knights FC", "Ember Bears FC", "Titan Rhinos FC", "Echo Ravens FC" };

static void print_bracket(Tournament *t) {
    char rn[32];
    for (int r = 1; r <= t->rounds; r++) {
        printf("  %s\n", bracket_round_name(t, r, rn, sizeof rn));
        for (int i = 1; i <= t->matchCount; i++) {
            Match *m = t->matches[i]; if (m->round != r) continue;
            const char *a = m->a ? m->a->name : "?", *b = m->b ? m->b->name : (m->status == M_BYE ? "(bye)" : "?");
            if (m->status == M_DONE) printf("    #%-2d %-18s %d - %d %-18s -> %s\n", m->id, a, m->scoreA, m->scoreB, b, m->winner->name);
            else if (m->status == M_BYE) printf("    #%-2d %-18s (bye)\n", m->id, a);
            else if (m->status == M_READY) printf("    #%-2d %-18s vs %-18s  ready\n", m->id, a, b);
            else printf("    #%-2d %-18s vs %-18s  (waiting)\n", m->id, a, b);
        }
    }
}

static void print_fans(Tournament *t) {
    Fan *fl[MAX_FANS]; int n = fan_leaderboard(t, fl, MAX_FANS);
    for (int i = 0; i < n; i++)
        printf("   %d. %-8s %3d points  (%d of %d predictions right)\n", i + 1, fl[i]->name, fan_points(t, fl[i]), fan_won(t, fl[i]), fan_total(t, fl[i]));
}

int main(int argc, char **argv) {
    int n = argc > 1 ? atoi(argv[1]) : 8; if (n < 2 || n > 32 || (n & (n - 1))) n = 8;   /* power of two only */
    rng_seed(argc > 2 ? (unsigned)atoi(argv[2]) : 1);
    char err[160];

    printf("== 1. CREATE cup and register clubs\n");
    Tournament *t = tournament_create("CLI Cup");
    for (int i = 0; i < n; i++) tournament_add_team(t, NAMES[i], 1300 + rng_next() % 500, err, sizeof err);
    printf("   %zu clubs in the linked list, %zu in the hash map; duplicate check: %s\n", t->teams.size, t->teamIndex.size,
           tournament_add_team(t, "neon falcons fc", 1500, err, sizeof err) ? err : "?");

    printf("== 2. SQUADS (linked list per club, hash map across the cup)\n");
    LIST_FOREACH(c, &t->teams) player_fill_squad(t, ((Team *)c->data)->id, err, sizeof err);
    Team *first = t->teams.head->data;
    printf("   %s squad:", first->name);
    LIST_FOREACH(p, &first->squad) printf(" %s#%d", position_name(((Player *)p->data)->pos), ((Player *)p->data)->number);
    printf("\n   %zu players indexed; duplicate player check: %s\n", t->playerIndex.size,
           player_add(t, first->id, ((Player *)first->squad.head->data)->name, POS_MID, 50, err, sizeof err) ? err : "?");

    printf("== 3. GENERATE bracket (heap seeding + queue-built tree)\n");
    tournament_generate(t, err, sizeof err);
    print_bracket(t);

    printf("== 4. FANS predict the next match with virtual points\n");
    fan_add(t, "Aarav", err, sizeof err); fan_add(t, "Maya", err, sizeof err);
    Match *up[8]; upnext_list(t, up, 8);
    predict_place(t, 1, up[0]->id, up[0]->a->id, 30, err, sizeof err);
    predict_place(t, 2, up[0]->id, up[0]->b->id, 20, err, sizeof err);
    printf("   Aarav backs %s, Maya backs %s on match #%d\n", up[0]->a->name, up[0]->b->name, up[0]->id);

    printf("== 5. PLAY the first match, winner advances, points are settled\n");
    tournament_sim(t, 0, err, sizeof err);
    Result *r = stack_peek(&t->undo);
    printf("   #%d: %s %d-%d %s  (next: match #%d)\n", r->matchId, r->nameA, r->scoreA, r->scoreB, r->nameB, r->nextMatchId);
    print_fans(t);

    printf("== 6. UNDO via stack + replay (points go back too)\n");
    tournament_undo(t, err, sizeof err);
    print_fans(t);

    printf("== 7. SIMULATE the rest\n");
    tournament_sim_all(t, err, sizeof err);
    print_bracket(t);
    print_fans(t);

    printf("== 8. STANDINGS (max-heap)\n");
    Team *out[MAX_TEAMS]; int k = ranking_standings(t, out, MAX_TEAMS);
    for (int i = 0; i < k; i++) printf("   %2d. %-18s %d-%d  Elo %.0f\n", i + 1, out[i]->name, out[i]->wins, out[i]->losses, out[i]->rating);
    printf("\nChampion: %s\n", t->champion ? t->champion->name : "-");
    tournament_destroy(t);
    return 0;
}
