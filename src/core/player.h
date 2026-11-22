/* player.h - club squads. Each club owns a linked list of players; a hash map gives
 * O(1) lookup by name across the whole tournament and blocks duplicate players.
 * (No player rankings or scorer tables: players are registered and listed only.) Owner: Member A */
#ifndef PLAYER_H
#define PLAYER_H
#include "core/types.h"

const char *position_name(Position p);              /* "GK" "DEF" "MID" "FWD" */
int         position_parse(const char *s, Position *out);   /* 0 on success */

int     player_add(Tournament *t, int team_id, const char *name, Position pos, int number, char *err, size_t ecap);
int     player_remove(Tournament *t, int player_id, char *err, size_t ecap);
Player *player_find(Tournament *t, const char *name);       /* O(1) average */
Player *player_by_id(Tournament *t, int id);
void    player_unindex_squad(Tournament *t, Team *tm);      /* used when a club is removed */
int     player_fill_squad(Tournament *t, int team_id, char *err, size_t ecap); /* sample 11-a-side squad */

#endif
