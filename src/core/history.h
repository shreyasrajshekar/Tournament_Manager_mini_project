/* history.h - event feed (linked list) and result records (undo stack). Owner: Member B */
#ifndef HISTORY_H
#define HISTORY_H
#include "core/types.h"

void event_log(Tournament *t, const char *kind, const char *fmt, ...);
void event_free(void *e);
void history_free_all(Tournament *t);   /* frees every Result on the undo stack */

#endif
