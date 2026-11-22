/* predict.h - fan predictions with VIRTUAL POINTS ONLY (no real money).
 *
 * Every fan starts with 100 points. A fan stakes points on the winner of a match that is
 * ready to play. If the pick wins, the fan gets `payout` points back (underdogs pay more,
 * based on the Elo win chance); otherwise the stake is lost.
 *
 * Points are always recomputed from the prediction list and the match results, so undoing
 * a result automatically takes the points back. Owner: Member C */
#ifndef PREDICT_H
#define PREDICT_H
#include "core/types.h"

typedef enum { PRED_OPEN, PRED_WON, PRED_LOST } PredState;

int  fan_add(Tournament *t, const char *name, char *err, size_t ecap);
Fan *fan_find(Tournament *t, const char *name);
Fan *fan_by_id(Tournament *t, int id);

/* Place a prediction. Only for a READY match, one per fan per match. */
int  predict_place(Tournament *t, int fan_id, int match_id, int pick_team_id, int stake, char *err, size_t ecap);
/* Same, but without readiness checks and with a stored payout: used when loading a save. */
int  predict_restore(Tournament *t, int fan_id, int match_id, int pick_team_id, int stake, int payout);

PredState predict_state(const Tournament *t, const Prediction *p);
int  fan_points(const Tournament *t, const Fan *f);       /* current balance */
int  fan_open_stakes(const Tournament *t, const Fan *f);  /* points currently at risk */
int  fan_won(const Tournament *t, const Fan *f);          /* number of correct predictions */
int  fan_total(const Tournament *t, const Fan *f);        /* number of predictions made */
int  predict_payout_for(const Tournament *t, const Match *m, int pick_team_id, int stake);

/* Fan leaderboard (max-heap on points). Returns the number of fans written. */
int  fan_leaderboard(Tournament *t, Fan **out, int cap);

/* After an undo: drop predictions on matches that are no longer playable (stakes return automatically). */
int  predict_prune(Tournament *t);

/* Called after a result is applied: logs who won or lost points in the activity feed. */
void predict_settle_events(Tournament *t, const Match *m);

#endif
