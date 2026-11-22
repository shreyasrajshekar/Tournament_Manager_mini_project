#include "ds/hashmap.h"
#include "util/util.h"
#include <stdlib.h>
#include <string.h>

unsigned hashmap_hash(const char *s) {
    unsigned h = 2166136261u;
    for (; *s; s++) { h ^= (unsigned char)*s; h *= 16777619u; }
    return h;
}
void hashmap_init(HashMap *m, size_t n) {
    if (n < 4) n = 4;
    m->nbuckets = n; m->size = m->collisions = 0;
    m->buckets = xcalloc(n, sizeof(HEntry *));
}
static void rehash(HashMap *m) {
    size_t nn = m->nbuckets * 2;
    HEntry **nb = xcalloc(nn, sizeof(HEntry *));
    for (size_t i = 0; i < m->nbuckets; i++) {
        HEntry *e = m->buckets[i];
        while (e) { HEntry *nx = e->next; size_t b = hashmap_hash(e->key) % nn; e->next = nb[b]; nb[b] = e; e = nx; }
    }
    free(m->buckets); m->buckets = nb; m->nbuckets = nn;
    trace_log("[HASH] rehash: buckets %zu -> %zu (load factor exceeded 0.75)", nn / 2, nn);
}
int hashmap_put(HashMap *m, const char *key, void *val) {
    size_t b = hashmap_hash(key) % m->nbuckets;
    for (HEntry *e = m->buckets[b]; e; e = e->next)
        if (strcmp(e->key, key) == 0) { e->val = val; return 0; }
    HEntry *e = xmalloc(sizeof *e);
    e->key = xstrdup(key); e->val = val;
    if (m->buckets[b]) m->collisions++;
    e->next = m->buckets[b]; m->buckets[b] = e; m->size++;
    trace_log("[HASH] put '%s' -> bucket %zu/%zu (chain length %zu)", key, b, m->nbuckets, hashmap_bucket_len(m, b));
    if (m->size * 4 > m->nbuckets * 3) rehash(m);
    return 1;
}
void *hashmap_get(const HashMap *m, const char *key) {
    size_t b = hashmap_hash(key) % m->nbuckets;
    for (HEntry *e = m->buckets[b]; e; e = e->next) if (strcmp(e->key, key) == 0) return e->val;
    return NULL;
}
int hashmap_remove(HashMap *m, const char *key) {
    size_t b = hashmap_hash(key) % m->nbuckets;
    HEntry **pp = &m->buckets[b];
    while (*pp) {
        if (strcmp((*pp)->key, key) == 0) {
            HEntry *d = *pp; *pp = d->next; free(d->key); free(d); m->size--;
            trace_log("[HASH] remove '%s' from bucket %zu", key, b);
            return 1;
        }
        pp = &(*pp)->next;
    }
    return 0;
}
void hashmap_free(HashMap *m) {
    for (size_t i = 0; i < m->nbuckets; i++) {
        HEntry *e = m->buckets[i];
        while (e) { HEntry *nx = e->next; free(e->key); free(e); e = nx; }
    }
    free(m->buckets); m->buckets = NULL; m->nbuckets = m->size = 0;
}
size_t hashmap_bucket_len(const HashMap *m, size_t i) {
    size_t n = 0; for (HEntry *e = m->buckets[i]; e; e = e->next) n++; return n;
}
size_t hashmap_max_chain(const HashMap *m) {
    size_t mx = 0;
    for (size_t i = 0; i < m->nbuckets; i++) { size_t n = hashmap_bucket_len(m, i); if (n > mx) mx = n; }
    return mx;
}
