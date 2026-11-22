#include "core/team.h"
#include "util/util.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

Team *team_new(int id, const char *name, double rating) {
    Team *t = xcalloc(1, sizeof *t);
    t->id = id; strncpy(t->name, name, TEAM_NAME_MAX - 1);
    t->rating0 = t->rating = rating;
    t->alive = 1;
    list_init(&t->history); list_init(&t->squad);
    return t;
}
void team_free(void *p) { Team *t = p; list_clear(&t->history, NULL); list_clear(&t->squad, free); free(t); }

void team_reset_stats(Team *t) {
    t->wins = t->losses = t->byes = t->pointsFor = t->pointsAgainst = 0;
    t->reached = 0; t->alive = 1; t->rating = t->rating0;
    list_clear(&t->history, NULL);
}
double elo_expected(double ra, double rb) { return 1.0 / (1.0 + pow(10.0, (rb - ra) / 400.0)); }
void elo_update(Team *a, Team *b, int a_won) {
    double ea = elo_expected(a->rating, b->rating);
    double d = 32.0 * ((a_won ? 1.0 : 0.0) - ea);
    a->rating += d; b->rating -= d;
}
