/* ranking.h - leaderboards built with max-heaps. Owner: Member B */
#ifndef RANKING_H
#define RANKING_H
#include "core/types.h"

/* Both return the number of teams written to out[] (best first). */
int ranking_power(Tournament *t, Team **out, int cap);      /* by live Elo rating */
int ranking_standings(Tournament *t, Team **out, int cap);  /* by tournament progress */
int ranking_cmp_power(const void *a, const void *b);        /* exposed for the DS inspector */

#endif
