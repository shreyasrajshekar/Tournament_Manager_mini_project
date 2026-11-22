/* db.h - SQLite storage for the tournament. The replay log is kept row-by-row in
 * `log` (the source of truth that is reloaded), and `teams`, `results`, `fans`
 * mirror it as ordinary tables so the data can be queried with any SQLite tool. */
#ifndef DB_H
#define DB_H
#include "core/types.h"

int         db_save(const Tournament *t, const char *path);                 /* 0 = ok */
Tournament *db_load(const char *path, char *err, size_t ecap);              /* NULL if empty or missing */

#endif
