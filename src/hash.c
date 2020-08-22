#include "mem.h"
#include "hash.h"
#include "types.h"

#include <stdio.h> // Debug purposes

#define LOAD_FACTOR 0.7

#define set(f)   h->f = 1;
#define unset(f) h->f = 0;

void h_init(hash_t *h) {
    h->n   = 0;
    h->an  = 0;
    h->cap = 0;
    h->lx  = 0;
    h->e   = NULL;
}

rf_int h_length(hash_t *h) {
    if (!h->lx)
        return h->n;
    rf_int l = 0;
    for (int i = 0; i < h->cap; ++i) {
        if (h->e[i] && !is_null(h->e[i]->val))
            ++l;
    }
    h->n = l;
    unset(lx);
    return l;
}

static entry_t *new_entry(rf_str *k, rf_val *v) {
    rf_str *nk = s_newstr(k->str, k->l, 0);
    nk->hash = k->hash;
    rf_val *nv;
    switch (v->type) {
    case TYPE_NULL: nv = v_newnull();      break;
    case TYPE_INT:  nv = v_newint(v->u.i); break;
    case TYPE_FLT:  nv = v_newflt(v->u.f); break;
    case TYPE_STR:  nv = v_newstr(v->u.s); break;
    case TYPE_ARR:
        nv      = v_newarr();
        nv->u.a = v->u.a;
        break;
    case TYPE_FN:
        nv = v; // Don't allocate new functions
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

static int exists(hash_t *h, rf_str *k) {
    if (!k->hash)
        k->hash = s_hash(k->str);
    int i = index(h->e, h->cap, k->hash);
    return h->e[i] && h->e[i]->key->hash == k->hash;
}

rf_val *h_lookup(hash_t *h, rf_str *k, int set) {
    if (set) set(lx);
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

rf_val *h_insert(hash_t *h, rf_str *k, rf_val *v, int set) {
    if (set) set(lx);
    if (!k->hash)
        k->hash = s_hash(k->str);
    // Evaluate hash table size
    if ((h->cap * LOAD_FACTOR) <= h->an + 1) {
        int new_cap = h->cap < 8 ? 8 : h->cap * 2;
        entry_t **new_e = malloc(new_cap * sizeof(entry_t *));
        for (int i = 0; i < new_cap; ++i)
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
        *h->e[i]->val = *v;
    }
    return h->e[i]->val;
}

// Remove the key/value pair by nullifying its slot
rf_val *h_delete(hash_t *h, rf_str *k) {
    if (!h->e || !exists(h, k))
        return NULL;
    if (!k->hash)
        k->hash = s_hash(k->str);
    int idx = index(h->e, h->cap, k->hash);
    rf_val *v = h->e[idx]->val;
    h->an--;
    if (!is_null(v))
        h->n--;
    free(h->e[idx]->key);
    h->e[idx] = NULL;
    return v;
}

#undef LOAD_FACTOR
#undef set
#undef unset
