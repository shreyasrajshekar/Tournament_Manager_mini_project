#include "server/json.h"
#include "util/util.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct { const char *p; } P;
static void ws(P *j) { while (*j->p && isspace((unsigned char)*j->p)) j->p++; }
static JVal *val(P *j, int depth);

static JVal *mk(JType t) { JVal *v = xcalloc(1, sizeof *v); v->t = t; return v; }

static char *parse_str(P *j) {            /* j->p at opening quote */
    j->p++;
    size_t cap = 32, n = 0; char *s = xmalloc(cap);
    while (*j->p && *j->p != '"') {
        char c = *j->p++;
        if (c == '\\') {
            char e = *j->p++;
            switch (e) {
                case 'n': c = '\n'; break; case 't': c = '\t'; break; case 'r': c = '\r'; break;
                case 'b': c = '\b'; break; case 'f': c = '\f'; break;
                case 'u': {
                    unsigned cp = 0;
                    for (int i = 0; i < 4 && isxdigit((unsigned char)*j->p); i++) { char h = *j->p++; cp = cp * 16 + (unsigned)(isdigit((unsigned char)h) ? h - '0' : (tolower(h) - 'a' + 10)); }
                    if (n + 4 >= cap) { cap *= 2; s = xrealloc(s, cap); }
                    if (cp < 0x80) s[n++] = (char)cp;
                    else if (cp < 0x800) { s[n++] = (char)(0xC0 | cp >> 6); s[n++] = (char)(0x80 | (cp & 63)); }
                    else { s[n++] = (char)(0xE0 | cp >> 12); s[n++] = (char)(0x80 | ((cp >> 6) & 63)); s[n++] = (char)(0x80 | (cp & 63)); }
                    continue;
                }
                case 0: free(s); return NULL;
                default: c = e;
            }
        }
        if (n + 2 >= cap) { cap *= 2; s = xrealloc(s, cap); }
        s[n++] = c;
    }
    if (*j->p != '"') { free(s); return NULL; }
    j->p++; s[n] = 0;
    return s;
}

static JVal *val(P *j, int depth) {
    if (depth > 32) return NULL;
    ws(j);
    char c = *j->p;
    if (c == '"') { char *s = parse_str(j); if (!s) return NULL; JVal *v = mk(J_STR); v->str = s; return v; }
    if (c == '{' || c == '[') {
        int obj = c == '{'; j->p++;
        JVal *v = mk(obj ? J_OBJ : J_ARR);
        ws(j);
        if (*j->p == (obj ? '}' : ']')) { j->p++; return v; }
        for (;;) {
            ws(j);
            char *key = NULL;
            if (obj) { if (*j->p != '"') goto bad; key = parse_str(j); if (!key) goto bad; ws(j); if (*j->p++ != ':') { free(key); goto bad; } }
            JVal *it = val(j, depth + 1);
            if (!it) { free(key); goto bad; }
            v->items = xrealloc(v->items, (size_t)(v->n + 1) * sizeof(JVal *));
            v->items[v->n] = it;
            if (obj) { v->keys = xrealloc(v->keys, (size_t)(v->n + 1) * sizeof(char *)); v->keys[v->n] = key; }
            v->n++;
            ws(j);
            if (*j->p == ',') { j->p++; continue; }
            if (*j->p == (obj ? '}' : ']')) { j->p++; return v; }
            goto bad;
        }
    bad:
        json_free(v); return NULL;
    }
    if (!strncmp(j->p, "true", 4)) { j->p += 4; JVal *v = mk(J_BOOL); v->b = 1; return v; }
    if (!strncmp(j->p, "false", 5)) { j->p += 5; return mk(J_BOOL); }
    if (!strncmp(j->p, "null", 4)) { j->p += 4; return mk(J_NULL); }
    if (c == '-' || isdigit((unsigned char)c)) { char *end; double d = strtod(j->p, &end); if (end == j->p) return NULL; j->p = end; JVal *v = mk(J_NUM); v->num = d; return v; }
    return NULL;
}

JVal *json_parse(const char *text) {
    P j = { text };
    JVal *v = val(&j, 0);
    return v;
}
void json_free(JVal *v) {
    if (!v) return;
    for (int i = 0; i < v->n; i++) { json_free(v->items[i]); if (v->keys) free(v->keys[i]); }
    free(v->items); free(v->keys); free(v->str); free(v);
}
JVal *json_get(const JVal *o, const char *key) {
    if (!o || o->t != J_OBJ) return NULL;
    for (int i = 0; i < o->n; i++) if (!strcmp(o->keys[i], key)) return o->items[i];
    return NULL;
}
double json_num(const JVal *o, const char *key, double d) {
    JVal *v = json_get(o, key);
    if (!v) return d;
    if (v->t == J_NUM) return v->num;
    if (v->t == J_STR) return atof(v->str);
    return d;
}
const char *json_str(const JVal *o, const char *key, const char *d) {
    JVal *v = json_get(o, key);
    return v && v->t == J_STR ? v->str : d;
}
