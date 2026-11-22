#include "ds/stack.h"
#include "util/util.h"
#include <stdlib.h>

void stack_init(Stack *s) { s->cap = 8; s->size = 0; s->data = xmalloc(s->cap * sizeof(void *)); }
void stack_push(Stack *s, void *item) {
    if (s->size == s->cap) { s->cap *= 2; s->data = xrealloc(s->data, s->cap * sizeof(void *)); }
    s->data[s->size++] = item;
}
void *stack_pop(Stack *s) { return s->size ? s->data[--s->size] : NULL; }
void *stack_peek(const Stack *s) { return s->size ? s->data[s->size - 1] : NULL; }
size_t stack_size(const Stack *s) { return s->size; }
void stack_clear(Stack *s, void (*free_fn)(void *)) {
    if (free_fn) for (size_t i = 0; i < s->size; i++) free_fn(s->data[i]);
    s->size = 0;
}
void stack_free(Stack *s) { free(s->data); s->data = NULL; s->size = s->cap = 0; }
