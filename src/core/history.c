#include "core/history.h"
#include "util/util.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void event_log(Tournament *t, const char *kind, const char *fmt, ...) {
    if (t->quiet) return;
    Event *e = xcalloc(1, sizeof *e);
    e->seq = ++t->eventSeq;
    strncpy(e->kind, kind, sizeof e->kind - 1);
    va_list ap; va_start(ap, fmt); vsnprintf(e->msg, sizeof e->msg, fmt, ap); va_end(ap);
    list_push_back(&t->events, e);
}
void event_free(void *e) { free(e); }

void history_free_all(Tournament *t) {
    stack_clear(&t->undo, free);
}
