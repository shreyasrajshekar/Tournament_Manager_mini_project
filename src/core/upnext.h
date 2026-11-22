/* upnext.h - which match is played next? A min-heap over the ready matches,
 * ordered by (round, position). No clock or court scheduling is involved. Owner: Member B */
#ifndef UPNEXT_H
#define UPNEXT_H
#include "core/types.h"

/* Writes up to cap ready matches into out[], earliest round first. Returns the count. */
int upnext_list(Tournament *t, Match **out, int cap);

#endif
