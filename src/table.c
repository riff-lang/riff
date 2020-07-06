#include "mem.h"
#include "table.h"

#define LOAD_FACTOR 0.7

void t_init(table_t *t) {
    t->n   = 0;
    t->cap = 0;
    t->e   = NULL;
}

#define probe

static int t_index(table_t *t, uint32_t key) {
    int i = key & (t->cap - 1);
}

val_t *t_lookup(table_t *t, str_t *k) {
}

void t_insert(table_t *t, str_t *k, val_t *v) {
    eval_resize_lf(t->e, t->n, t->cap, LOAD_FACTOR);
    // if ((t->cap * TBL_MAX_LOAD) <= t->n + 1) {
    //     t->cap = increase_cap(t->cap);
    //     t->e   = realloc(t->e, sizeof(entry_t *) * t->cap);
    // }

}

void t_delete(table_t *t, str_t *k) {
    t->n--;
}

#undef LOAD_FACTOR
