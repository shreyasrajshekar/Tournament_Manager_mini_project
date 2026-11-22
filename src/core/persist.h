/* persist.h - save/load as a replayable text log. Owner: Member B
 * Because state = (config, teams, ordered results), loading just re-plays it. */
#ifndef PERSIST_H
#define PERSIST_H
#include "core/types.h"
#include "util/util.h"

int         persist_save(const Tournament *t, const char *path);
void        persist_write(const Tournament *t, StrBuf *out);
Tournament *persist_load(const char *path, char *err, size_t ecap);
Tournament *persist_load_text(const char *text, char *err, size_t ecap);   /* same format, from memory */

#endif
