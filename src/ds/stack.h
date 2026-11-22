/* stack.h - array-backed LIFO stack of void*.
 * Used for: the undo stack of match results (event sourcing: pop the last
 * result, then replay the rest).
 * Owner: Member A */
#ifndef STACK_H
#define STACK_H
#include <stddef.h>

typedef struct { void **data; size_t size, cap; } Stack;

void   stack_init(Stack *s);
void   stack_push(Stack *s, void *item);   /* amortised O(1) */
void  *stack_pop(Stack *s);                /* O(1); NULL if empty */
void  *stack_peek(const Stack *s);
size_t stack_size(const Stack *s);
void   stack_clear(Stack *s, void (*free_fn)(void *));
void   stack_free(Stack *s);

#endif
