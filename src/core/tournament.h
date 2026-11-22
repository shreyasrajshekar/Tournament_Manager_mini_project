/* tournament.h - facade: the only API the server / CLI needs. Owner: Member B
 * All mutating calls return 0 on success, -1 on failure (err is filled). */
#ifndef TOURNAMENT_H
#define TOURNAMENT_H
#include "core/types.h"

Tournament *tournament_create(const char *name);
void        tournament_destroy(Tournament *t);

int tournament_add_team(Tournament *t, const char *name, double rating, char *err, size_t ecap);
int tournament_remove_team(Tournament *t, int team_id, char *err, size_t ecap);
int tournament_generate(Tournament *t, char *err, size_t ecap);

int tournament_play(Tournament *t, int match_id, int sa, int sb, char *err, size_t ecap);
int tournament_sim(Tournament *t, int match_id /*0 = next in schedule*/, char *err, size_t ecap);
int tournament_sim_round(Tournament *t, char *err, size_t ecap);
int tournament_sim_all(Tournament *t, char *err, size_t ecap);
int tournament_undo(Tournament *t, char *err, size_t ecap);

Team  *tournament_find_team(Tournament *t, const char *name);
Team  *tournament_team_by_id(Tournament *t, int id);
const char *phase_name(Phase p);

#endif
