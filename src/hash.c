#include "mem.h"
#include "hash.h"
#include "types.h"

#include <stdio.h> // Debug purposes

#define LOAD_FACTOR 0.7

void h_init(hash_t *h) {
    h->n   = 0;
    h->cap = 0;
    h->e   = NULL;
}

static entry_t *new_entry(str_t *k, val_t *v) {
    str_t *nk = s_newstr(k->str, k->l, 1);
    val_t *nv;
    switch (v->type) {
    case TYPE_VOID: nv = v_newvoid();      break;
    case TYPE_INT:  nv = v_newint(v->u.i); break;
    case TYPE_FLT:  nv = v_newflt(v->u.f); break;
    case TYPE_STR:  nv = v_newstr(v->u.s); break;
    default: break;
    }
    entry_t *e = malloc(sizeof(entry_t));
    e->key = nk;
    e->val = nv;
    return e;
}

// Given a string's hash, return index of the element if it exists in the
// table, or an index of a suitble empty slot
static int index(entry_t **e, int cap, uint32_t hash) {
    cap -= 1;
    int i = hash & cap;
    while (e[i] && e[i]->key->hash != hash) {
        i = (i + 1) & cap; // Linear probing
    }
    return i;
}

static int exists(hash_t *h, str_t *k) {
    if (!k->hash)
        k->hash = s_hash(k->str);
    int i = index(h->e, h->cap, k->hash);
    return h->e[i] && h->e[i]->key->hash == k->hash;
}

val_t *h_lookup(hash_t *h, str_t *k) {
    // If the table is empty, insert void value
    if (!h->e)
        h_insert(h, k, v_newvoid());
    if (!k->hash)
        k->hash = s_hash(k->str);
    int i = index(h->e, h->cap, k->hash);
    if (!h->e[i])
        h_insert(h, k, v_newvoid());
    return h->e[i]->val;
}

void h_insert(hash_t *h, str_t *k, val_t *v) {
    if (!k->hash)
        k->hash = s_hash(k->str);
    // Evaluate hash table size
    if ((h->cap * LOAD_FACTOR) <= h->n + 1) {
        int nc = h->cap < 8 ? 8 : h->cap * 2;
        entry_t **ne = malloc(nc * sizeof(entry_t *));
        for (int i = 0; i < nc; i++)
            ne[i] = NULL;
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
        h->e[i] = new_entry(k, v);
    } else {
        h->e[i]->val->type = v->type;
        h->e[i]->val->u    = v->u;
    }
}

// TODO
void h_delete(hash_t *h, str_t *k) {
    h->n--;
}

#undef LOAD_FACTOR
