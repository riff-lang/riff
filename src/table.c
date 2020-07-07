#include "mem.h"
#include "table.h"

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
    entry_t *e = malloc(sizeof(entry_t));
    e->key = nk;
    e->val = nv;
    return e;
}

// Given a string's hash, return index of the element exists in the
// table, or an index of a suitble empty slot
static int t_index(entry_t **e, int cap, uint32_t key) {
    int i = key & (cap - 1);
    while (e[i] && e[i]->key->hash != key) {
        i = (i << 1) & (cap - 1); // Quadratic probing
    }
    return i;
}

static int t_exists(table_t *t, str_t *k) {
    int i = t_index(t->e, t->cap, k->hash);
    return t->e[i] && t->e[i]->key->hash == k->hash;
}

val_t *t_lookup(table_t *t, str_t *k) {
    int i = t_index(t->e, t->cap, k->hash);
    if (!t->e[i])
        return v_newint(0);
    else
        return t->e[i]->val;
}

void t_insert(table_t *t, str_t *k, val_t *v) {
    // Evaluate hash table size
    if ((t->cap * LOAD_FACTOR) <= t->n + 1) {
        int nc = t->cap < 8 ? 8 : t->cap * 2;
        entry_t **ne = calloc(nc, sizeof(entry_t *));
        int nn = 0;
        int j;
        if (t->n) {
            for (int i = 0; t->n || i < t->cap; i++) {
                if (!t->e[i]) {
                    continue;
                } else {
                    j = t_index(ne, nc, t->e[i]->key->hash);
                    ne[j] = t->e[i];
                    nn++;
                    t->n--;
                }
            }
        }
        free(t->e);
        t->e   = ne;
        t->n   = nn;
        t->cap = nc;
    }
    int i = t_index(t->e, t->cap, k->hash);
    if (!t_exists(t, k)) {
        t->n++;
        free(t->e[i]);
    }
    t->e[i] = new_entry(k, v);
}

// TODO
void t_delete(table_t *t, str_t *k) {
    t->n--;
}

#undef LOAD_FACTOR
