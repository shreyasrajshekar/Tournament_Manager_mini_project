/* hashmap.h - separate-chaining hash table, string keys -> void* values.
 * FNV-1a hash, doubles bucket count when load factor > 0.75.
 * Used for: O(1) team lookup by (case-insensitive) name and duplicate
 * detection at registration.
 * Owner: Member A */
#ifndef HASHMAP_H
#define HASHMAP_H
#include <stddef.h>

typedef struct HEntry { char *key; void *val; struct HEntry *next; } HEntry;
typedef struct { HEntry **buckets; size_t nbuckets, size, collisions; } HashMap;

void   hashmap_init(HashMap *m, size_t initial_buckets);
int    hashmap_put(HashMap *m, const char *key, void *val); /* 1 = new key, 0 = replaced */
void  *hashmap_get(const HashMap *m, const char *key);      /* NULL if absent */
int    hashmap_remove(HashMap *m, const char *key);         /* 1 = removed */
void   hashmap_free(HashMap *m);                            /* frees keys, not values */

unsigned hashmap_hash(const char *key);
size_t hashmap_bucket_len(const HashMap *m, size_t i);      /* chain length (for the inspector) */
size_t hashmap_max_chain(const HashMap *m);

#endif
