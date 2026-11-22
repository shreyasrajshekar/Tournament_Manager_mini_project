/* report.h - printable end-of-tournament report (self-contained HTML). */
#ifndef REPORT_H
#define REPORT_H
#include "core/types.h"
#include "util/util.h"
void report_html(Tournament *t, StrBuf *out);
#endif
