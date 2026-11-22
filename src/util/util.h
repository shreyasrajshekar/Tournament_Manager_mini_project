/* util.h - small shared helpers: memory, strings, string builder, RNG, trace log.
 * Owner: Member A (data-structure foundations) */
#ifndef UTIL_H
#define UTIL_H
#include <stddef.h>

void *xmalloc(size_t n);
void *xcalloc(size_t n, size_t sz);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);

void str_lower(char *dst, size_t cap, const char *src);
void str_trim(char *s);
void str_sanitize(char *s);               /* replaces '|' and control chars with ' ' */
void fmt_clock(int minutes, char *buf, size_t cap); /* 545 -> "09:05" (+1d if >= 24h) */

/* ---- growable string builder (used for JSON output) ---- */
typedef struct { char *data; size_t len, cap; } StrBuf;
void sb_init(StrBuf *sb);
void sb_free(StrBuf *sb);
void sb_puts(StrBuf *sb, const char *s);
void sb_printf(StrBuf *sb, const char *fmt, ...);
void sb_json_str(StrBuf *sb, const char *s); /* quoted + escaped */

/* ---- tiny xorshift RNG ---- */
void rng_seed(unsigned s);
unsigned rng_next(void);
double rng_double(void);

/* ---- operation trace: data-structure modules log what they do here,
 *      the UI shows it in the "C core trace" console ---- */
int  trace_enable(int on);               /* returns previous state */
void trace_clear(void);
void trace_log(const char *fmt, ...);
int  trace_count(void);
const char *trace_at(int i);

#endif
