#include "mem.h"
#include "hash.h"
#include "types.h"

#include <stdio.h> // Debug purposes

#define LOAD_FACTOR 0.7

void h_init(hash_t *h) {
    h->n   = 0;
    h->an  = 0;
    h->cap = 0;
    h->e   = NULL;
}

static entry_t *new_entry(str_t *k, val_t *v) {
    str_t *nk = s_newstr(k->str, k->l, 0);
    nk->hash = k->hash;
    val_t *nv;
    switch (v->type) {
    case TYPE_NULL: nv = v_newnull();      break;
    case TYPE_INT:  nv = v_newint(v->u.i); break;
    case TYPE_FLT:  nv = v_newflt(v->u.f); break;
    case TYPE_STR:  nv = v_newstr(v->u.s); break;
    case TYPE_ARR:
        nv = v_newarr();
        nv->type = TYPE_ARR;
        nv->u.a  = v->u.a;
        break;
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

val_t *h_lookup(hash_t *h, str_t *k, int set) {
    // If the table is empty, call h_insert, which allocates memory
    if (!h->e)
        return h_insert(h, k, v_newnull(), set);
    if (!k->hash)
        k->hash = s_hash(k->str);
    int i = index(h->e, h->cap, k->hash);
    if (!h->e[i])
        return h_insert(h, k, v_newnull(), set);
    return h->e[i]->val;
}

val_t *h_insert(hash_t *h, str_t *k, val_t *v, int set) {
    if (!k->hash)
        k->hash = s_hash(k->str);
    // Evaluate hash table size
    if ((h->cap * LOAD_FACTOR) <= h->an + 1) {
        int new_cap = h->cap < 8 ? 8 : h->cap * 2;
        entry_t **new_e = malloc(new_cap * sizeof(entry_t *));
        for (int i = 0; i < new_cap; i++)
            new_e[i] = NULL;
        int new_n  = 0;
        int new_an = 0;
        int old_an = h->an;
        int j;
        if (old_an) {
            for (int i = 0; old_an || i < h->cap; i++) {
                if (!h->e[i]) {
                    continue;
                } else {
                    j = index(new_e, new_cap, h->e[i]->key->hash);
                    new_e[j] = h->e[i];
                    ++new_an;
                    --old_an;
                    if (!is_null(h->e[i]->val))
                        ++new_n;
                }
            }
        }
        free(h->e);
        h->e   = new_e;
        h->n   = new_n;
        h->an  = new_an;
        h->cap = new_cap;
    }
    int i = index(h->e, h->cap, k->hash);
    if (!exists(h, k)) {
        free(h->e[i]);
        h->e[i] = new_entry(k, v);
        h->an++;
        if (set || !is_null(v))
            h->n++;
    } else {
        h->e[i]->val->type = v->type;
        h->e[i]->val->u    = v->u;
    }
    return h->e[i]->val;
}

// TODO
void h_delete(hash_t *h, str_t *k) {
    h->n--;
}

#undef LOAD_FACTOR
