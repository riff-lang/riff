#include "mem.h"
#include "hash.h"
#include "types.h"

#define LOAD_FACTOR 0.7

void h_init(hash_t *h) {
    h->n   = 0;
    h->cap = 8;
    h->e   = calloc(8, sizeof(entry_t *));
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
    entry_t *e = malloc(sizeof(entry_t *));
    e->key = nk;
    e->val = nv;
    return e;
}

// Given a string's hash, return index of the element exists in the
// hash, or an index of a suitble empty slot
static int index(entry_t **e, int cap, uint32_t key) {
    int i = key & (cap - 1);
    while (e[i] && e[i]->key->hash != key) {
        i = (i << 1) & (cap - 1); // Quadratic probing
    }
    return i;
}

static int exists(hash_t *h, str_t *k) {
    int i = index(h->e, h->cap, k->hash);
    return h->e[i] && h->e[i]->key->hash == k->hash;
}

val_t *h_lookup(hash_t *h, str_t *k) {
    int i = index(h->e, h->cap, k->hash);
    return !h->e[i] ? v_newvoid() : h->e[i]->val;
}

void h_insert(hash_t *h, str_t *k, val_t *v) {
    // Evaluate hash hash size
    if ((h->cap * LOAD_FACTOR) <= h->n + 1) {
        int nc = h->cap < 8 ? 8 : h->cap * 2;
        entry_t **ne = calloc(nc, sizeof(entry_t *));
        int nn = 0;
        int j;
        if (h->n) {
            for (int i = 0; h->n || i < h->cap; i++) {
                if (!h->e[i]) {
                    continue;
                } else {
                    j = index(ne, nc, h->e[i]->key->hash);
                    ne[j] = h->e[i];
                    nn++;
                    h->n--;
                }
            }
        }
        free(h->e);
        h->e   = ne;
        h->n   = nn;
        h->cap = nc;
    }
    int i = index(h->e, h->cap, k->hash);
    if (!exists(h, k)) {
        h->n++;
        free(h->e[i]);
    }
    h->e[i] = new_entry(k, v);
}

// TODO
void h_delete(hash_t *h, str_t *k) {
    h->n--;
}

#undef LOAD_FACTOR
