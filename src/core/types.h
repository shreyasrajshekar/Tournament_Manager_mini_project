/* types.h - shared domain types for the tournament engine.
 * Every core module includes this; it is the "contract" between members. */
#ifndef TYPES_H
#define TYPES_H

#include "ds/hashmap.h"
#include "ds/linkedlist.h"
#include "ds/queue.h"
#include "ds/stack.h"
#include "ds/heap.h"

#define TEAM_NAME_MAX 48
#define MAX_TEAMS 64
#define PLAYER_NAME_MAX 40
#define MAX_SQUAD 25
#define FAN_NAME_MAX 32
#define MAX_FANS 64
#define FAN_START_POINTS 100

typedef enum { PHASE_SETUP, PHASE_RUNNING, PHASE_FINISHED } Phase;
typedef enum { M_PENDING, M_READY, M_DONE, M_BYE } MatchStatus;

typedef struct Result Result;

typedef enum { POS_GK, POS_DEF, POS_MID, POS_FWD } Position;

struct Team;
/* A player belongs to exactly one club's squad. */
typedef struct Player {
    int id;
    char name[PLAYER_NAME_MAX];
    Position pos;
    int number;                    /* shirt number, unique inside the club */
    struct Team *team;
} Player;

typedef struct Team {
    int id;
    char name[TEAM_NAME_MAX];
    int seed;                      /* 1 = best, assigned at bracket generation */
    double rating0, rating;        /* initial and live Elo rating */
    int wins, losses, byes;
    int pointsFor, pointsAgainst;
    int reached;                   /* furthest round the team has been placed in */
    int alive;
    List history;                  /* Result* (newest first, not owned) */
    List squad;                    /* Player*, registration order (owned) */
} Team;

/* A node of the bracket tree. Leaves of the *tree* are first-round matches;
 * their slots (a,b) are filled from the seeding order. Internal matches get
 * their slots filled when a child match finishes. */
typedef struct Match {
    int id, round, pos;            /* id is level-order; pos = index inside the round */
    struct Match *left, *right, *parent;
    Team *a, *b, *winner;
    int scoreA, scoreB;
    MatchStatus status;
} Match;

/* One played match, stored on the undo stack. */
struct Result {
    int seq, matchId, round, nextMatchId;
    int teamA, teamB, winnerId;
    char nameA[TEAM_NAME_MAX], nameB[TEAM_NAME_MAX];
    int scoreA, scoreB;
    double ratingA_before, ratingA_after, ratingB_before, ratingB_after;
};

typedef struct { int seq; char kind[16]; char msg[220]; } Event;

/* A fan who predicts match winners using virtual points only (no real money). */
typedef struct Fan {
    int id;
    char name[FAN_NAME_MAX];
} Fan;

/* One prediction. `payout` is the number of points returned if the pick wins; it is
 * fixed at placement time from the Elo win chance (underdogs pay more). */
typedef struct Prediction {
    int id, fanId, matchId, pickId, stake, payout;
} Prediction;

typedef struct Tournament {
    char name[64];
    Phase phase;

    List teams;                    /* Team*, registration order            (linked list) */
    HashMap teamIndex;             /* lower(name) -> Team*                 (hash map)    */
    int nextTeamId;

    Match *root;                   /* bracket                              (binary tree) */
    Match **matches;               /* by id, index 1..matchCount */
    int matchCount, rounds, bracketSize;
    Team *champion;

    HashMap playerIndex;           /* lower(name) -> Player*               (hash map)    */
    int nextPlayerId;

    List fans;                     /* Fan*                                 (linked list) */
    HashMap fanIndex;              /* lower(name) -> Fan*                  (hash map)    */
    List predictions;              /* Prediction*, placement order         (linked list) */
    HashMap predIndex;             /* "fanId:matchId" -> Prediction*       (hash map)    */
    int nextFanId, nextPredId;

    Stack undo;                    /* Result*                              (stack) */
    int resultSeq;
    List events; int eventSeq;     /* Event*, append-only feed             (linked list) */
    int quiet;                     /* 1 while replaying: no events */
} Tournament;

#endif
