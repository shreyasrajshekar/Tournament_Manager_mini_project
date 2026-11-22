/* bracket.h - single-elimination bracket as a binary tree. Owner: Member B
 *
 *  seeding : team heap (max-heap on rating) -> seeds 1..N
 *  build   : queue of match nodes, merged pairwise level by level
 *  advance : winner is written into parent slot (O(1) via parent pointer)
 *  export  : level-order traversal using a queue */
#ifndef BRACKET_H
#define BRACKET_H
#include "core/types.h"

void   bracket_seed_teams(Tournament *t);              /* heap-sort teams by rating -> t->teams seeds */
void   bracket_build(Tournament *t);                   /* builds tree, resolves byes, queues ready matches */
void   bracket_free(Tournament *t);
void   bracket_advance(Tournament *t, Match *m);       /* push winner of m into its parent */
int    bracket_level_order(Tournament *t, Match **out, int cap); /* BFS from root */
const char *bracket_round_name(const Tournament *t, int round, char *buf, size_t cap);

#endif
