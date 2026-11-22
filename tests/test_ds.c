/* Unit tests for the generic containers (Member A). */
#include "ds/hashmap.h"
#include "ds/linkedlist.h"
#include "ds/queue.h"
#include "ds/stack.h"
#include "ds/heap.h"
#include "util/util.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int cmp_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

int main(void) {
    trace_enable(0);
    /* hashmap: insert 500 keys (forces rehash), lookup, remove */
    HashMap m; hashmap_init(&m, 4);
    static int vals[500]; char k[32];
    for (int i = 0; i < 500; i++) { vals[i] = i; sprintf(k, "key%d", i); assert(hashmap_put(&m, k, &vals[i]) == 1); }
    assert(m.size == 500 && m.nbuckets >= 512);
    assert(hashmap_put(&m, "key7", &vals[8]) == 0 && *(int *)hashmap_get(&m, "key7") == 8);
    for (int i = 0; i < 500; i += 2) { sprintf(k, "key%d", i); assert(hashmap_remove(&m, k)); }
    assert(m.size == 250 && !hashmap_get(&m, "key0") && hashmap_get(&m, "key1"));
    assert(!hashmap_remove(&m, "nope"));
    hashmap_free(&m);

    /* linked list */
    List l; list_init(&l); int a = 1, b = 2, c = 3;
    list_push_back(&l, &a); ListNode *nb = list_push_back(&l, &b); list_push_front(&l, &c);
    assert(l.size == 3 && *(int *)l.head->data == 3 && *(int *)l.tail->data == 2);
    list_remove(&l, nb); assert(l.size == 2 && *(int *)l.tail->data == 1);
    assert(*(int *)list_pop_front(&l) == 3 && *(int *)list_pop_back(&l) == 1 && !list_pop_back(&l));
    list_clear(&l, NULL);

    /* queue: wrap-around + growth keeps FIFO order */
    Queue q; queue_init(&q); static int qv[100];
    for (int i = 0; i < 6; i++) { qv[i] = i; queue_push(&q, &qv[i]); }
    for (int i = 0; i < 4; i++) assert(*(int *)queue_pop(&q) == i);
    for (int i = 6; i < 100; i++) { qv[i] = i; queue_push(&q, &qv[i]); }
    for (int i = 4; i < 100; i++) assert(*(int *)queue_pop(&q) == i);
    assert(queue_size(&q) == 0 && !queue_pop(&q));
    queue_free(&q);

    /* stack */
    Stack s; stack_init(&s);
    for (int i = 0; i < 50; i++) stack_push(&s, &qv[i]);
    for (int i = 49; i >= 0; i--) assert(*(int *)stack_pop(&s) == i);
    assert(!stack_pop(&s)); stack_free(&s);

    /* heap: sorts 1000 random ints */
    Heap h; heap_init(&h, cmp_int, "t"); static int hv[1000];
    for (int i = 0; i < 1000; i++) { hv[i] = (int)(rng_next() % 5000); heap_push(&h, &hv[i]); }
    int prev = -1;
    for (int i = 0; i < 1000; i++) { int x = *(int *)heap_pop(&h); assert(x >= prev); prev = x; }
    assert(!heap_pop(&h)); heap_free(&h);

    puts("test_ds: all passed");
    return 0;
}
