#include "mem.h"
#include "table.h"
#include <stdio.h>
#include <stdint.h>

#define LOAD_FACTOR 0.7

void t_init(table_t *t) {
    t->n   = 0;
    t->cap = 0;
    t->e   = NULL;
}

static entry_t *new_entry(str_t *k, val_t *v) {
    str_t *nk = s_newstr(k->str, k->l);
    val_t *nv;
    switch (v->type) {
    case TYPE_INT: nv = v_newint(v->u.i); break;
    case TYPE_FLT: nv = v_newflt(v->u.f); break;
    case TYPE_STR: nv = v_newstr(v->u.s); break;
    default: break;
    }
    entry_t *e = malloc(sizeof(entry_t) + 1);
    e->key = nk;
    e->val = nv;
    return e;
}

// Return suitable index in the given hash table for a given key
static int t_index(table_t *t, uint32_t key) {
    int i = key & (t->cap - 1);
    printf("%d\n", i);
    while (t->e[i] != NULL && t->e[i]->key->hash != key) {
        i = (i + 1) & (t->cap - 1);
        printf("%d\n", t->cap);
    }
    return i;
}

val_t *t_lookup(table_t *t, str_t *k) {
    int i = t_index(t, k->hash);
    if (t->e[i] == NULL)
        return v_newint(0);
    else
        return t->e[i]->val;
}

void t_insert(table_t *t, str_t *k, val_t *v) {
    eval_resize_lf(t->e, t->n, t->cap, LOAD_FACTOR);
    // if ((t->cap * TBL_MAX_LOAD) <= t->n + 1) {
    //     t->cap = increase_cap(t->cap);
    //     t->e   = realloc(t->e, sizeof(entry_t *) * t->cap);
    // }
    int i = t_index(t, k->hash);
    t->e[i] = new_entry(k, v);

}

void t_delete(table_t *t, str_t *k) {
    t->n--;
}

#undef LOAD_FACTOR
#undef probe
