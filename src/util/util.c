#include "util/util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

void *xmalloc(size_t n) { void *p = malloc(n ? n : 1); if (!p) { fputs("out of memory\n", stderr); exit(1); } return p; }
void *xcalloc(size_t n, size_t sz) { void *p = calloc(n ? n : 1, sz ? sz : 1); if (!p) { fputs("out of memory\n", stderr); exit(1); } return p; }
void *xrealloc(void *p, size_t n) { p = realloc(p, n ? n : 1); if (!p) { fputs("out of memory\n", stderr); exit(1); } return p; }
char *xstrdup(const char *s) { size_t n = strlen(s) + 1; char *d = xmalloc(n); memcpy(d, s, n); return d; }

void str_lower(char *dst, size_t cap, const char *src) {
    size_t i = 0;
    for (; src[i] && i + 1 < cap; i++) dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = 0;
}
void str_trim(char *s) {
    size_t n = strlen(s), a = 0;
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = 0;
    while (s[a] && isspace((unsigned char)s[a])) a++;
    if (a) memmove(s, s + a, strlen(s + a) + 1);
}
void str_sanitize(char *s) {
    for (; *s; s++) if (*s == '|' || (unsigned char)*s < 32 || *s == '"' || *s == '\\') *s = ' ';
}
void fmt_clock(int m, char *buf, size_t cap) {
    int day = m / 1440; m %= 1440;
    if (day > 0) snprintf(buf, cap, "%02d:%02d +%dd", m / 60, m % 60, day);
    else snprintf(buf, cap, "%02d:%02d", m / 60, m % 60);
}

/* ---- StrBuf ---- */
void sb_init(StrBuf *sb) { sb->cap = 1024; sb->len = 0; sb->data = xmalloc(sb->cap); sb->data[0] = 0; }
void sb_free(StrBuf *sb) { free(sb->data); sb->data = NULL; sb->len = sb->cap = 0; }
static void sb_reserve(StrBuf *sb, size_t extra) {
    if (sb->len + extra + 1 <= sb->cap) return;
    while (sb->len + extra + 1 > sb->cap) sb->cap *= 2;
    sb->data = xrealloc(sb->data, sb->cap);
}
void sb_puts(StrBuf *sb, const char *s) { size_t n = strlen(s); sb_reserve(sb, n); memcpy(sb->data + sb->len, s, n + 1); sb->len += n; }
void sb_printf(StrBuf *sb, const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return; }
    sb_reserve(sb, (size_t)n);
    vsnprintf(sb->data + sb->len, (size_t)n + 1, fmt, ap2); va_end(ap2);
    sb->len += (size_t)n;
}
void sb_json_str(StrBuf *sb, const char *s) {
    sb_puts(sb, "\"");
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"') sb_puts(sb, "\\\"");
        else if (c == '\\') sb_puts(sb, "\\\\");
        else if (c == '\n') sb_puts(sb, "\\n");
        else if (c == '\r') sb_puts(sb, "\\r");
        else if (c == '\t') sb_puts(sb, "\\t");
        else if (c < 32) sb_printf(sb, "\\u%04x", c);
        else { char t[2] = { (char)c, 0 }; sb_puts(sb, t); }
    }
    sb_puts(sb, "\"");
}

/* ---- RNG ---- */
static unsigned rng_state = 2463534242u;
void rng_seed(unsigned s) { rng_state = s ? s : 2463534242u; }
unsigned rng_next(void) { unsigned x = rng_state; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return rng_state = x; }
double rng_double(void) { return (rng_next() & 0xFFFFFF) / (double)0x1000000; }

/* ---- trace ---- */
#define TRACE_MAX 400
#define TRACE_LEN 200
static char tr_lines[TRACE_MAX][TRACE_LEN];
static int tr_count = 0, tr_on = 1;
int trace_enable(int on) { int p = tr_on; tr_on = on; return p; }
void trace_clear(void) { tr_count = 0; }
void trace_log(const char *fmt, ...) {
    if (!tr_on || tr_count >= TRACE_MAX) return;
    va_list ap; va_start(ap, fmt);
    vsnprintf(tr_lines[tr_count++], TRACE_LEN, fmt, ap);
    va_end(ap);
}
int trace_count(void) { return tr_count; }
const char *trace_at(int i) { return (i >= 0 && i < tr_count) ? tr_lines[i] : ""; }
