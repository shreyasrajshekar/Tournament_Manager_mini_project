/* team.h - team objects and Elo maths. Owner: Member B */
#ifndef TEAM_H
#define TEAM_H
#include "core/types.h"

Team  *team_new(int id, const char *name, double rating);
void   team_free(void *team);
void   team_reset_stats(Team *t);
double elo_expected(double ra, double rb);          /* P(A beats B) */
void   elo_update(Team *a, Team *b, int a_won);     /* K = 32 */

#endif
