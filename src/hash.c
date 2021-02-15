#include "mem.h"
#include "hash.h"
#include "types.h"
#include "util.h"

#include <stdio.h> // Debug purposes

#define LOAD_FACTOR 0.6

#define set(f)   h->f = 1
#define unset(f) h->f = 0

void h_init(rf_htbl *h) {
    h->n     = 0;
    h->an    = 0;
    h->cap   = 0;
    h->lx    = 0;
    h->nodes = NULL;
}

uint32_t h_length(rf_htbl *h) {
    if (!h->lx)
        return h->n;
    uint32_t l = 0;
    for (int i = 0; i < h->cap; ++i) {
        if (h->nodes[i] && !is_null(h->nodes[i]->val))
            ++l;
    }
    h->n = l;
    unset(lx);
    return l;
}

static ht_node *new_node(rf_str *k, rf_val *v) {
    rf_str *nk = s_newstr(k->str, k->l, 0);
    nk->hash = k->hash;
    rf_val *nv;
    switch (v->type) {
    case TYPE_NULL: nv = v_newnull();      break;
    case TYPE_INT:  nv = v_newint(v->u.i); break;
    case TYPE_FLT:  nv = v_newflt(v->u.f); break;
    case TYPE_STR:  nv = v_newstr(v->u.s); break;
    case TYPE_TBL:
        nv      = v_newtbl();
        nv->u.t = v->u.t;
        break;
    case TYPE_RFN: case TYPE_CFN:
        nv = v; // Don't allocate new functions
        break;
    default: break;
    }
    ht_node *e = malloc(sizeof(ht_node));
    e->key = nk;
    e->val = nv;
    return e;
}

// Given a string's hash, return index of the element if it exists in the
// table, or an index of a suitble empty slot
static uint32_t node_slot(ht_node **e, uint32_t cap, uint32_t hash) {
    cap -= 1;
    uint32_t i = hash & cap;
    while (e[i] && e[i]->key->hash != hash) {
        i = (i + 1) & cap; // Linear probing
    }
    return i;
}

static int exists(rf_htbl *h, rf_str *k) {
    if (!k->hash)
        k->hash = u_strhash(k->str);
    int i = node_slot(h->nodes, h->cap, k->hash);
    return h->nodes[i] && h->nodes[i]->key->hash == k->hash;
}

rf_val *h_lookup(rf_htbl *h, rf_str *k, int set) {
    if (set) set(lx);
    // If the table is empty, call h_insert, which allocates memory
    if (!h->nodes)
        return h_insert(h, k, v_newnull(), set);
    if (!k->hash)
        k->hash = u_strhash(k->str);
    int i = node_slot(h->nodes, h->cap, k->hash);
    if (!h->nodes[i])
        return h_insert(h, k, v_newnull(), set);
    return h->nodes[i]->val;
}

rf_val *h_insert(rf_htbl *h, rf_str *k, rf_val *v, int set) {
    if (set) set(lx);
    if (!k->hash)
        k->hash = u_strhash(k->str);
    // Evaluate hash table size
    if ((h->cap * LOAD_FACTOR) <= h->an + 1) {
        int new_cap = h->cap < 8 ? 8 : h->cap * 2;
        ht_node **new_nodes = malloc(new_cap * sizeof(ht_node *));
        for (int i = 0; i < new_cap; ++i)
            new_nodes[i] = NULL;
        int new_n  = 0;
        int new_an = 0;
        int old_an = h->an;
        int j;
        if (old_an) {
            for (int i = 0; old_an && (i < h->cap); i++) {
                if (!h->nodes[i]) {
                    continue;
                } else {
                    j = node_slot(new_nodes, new_cap, h->nodes[i]->key->hash);
                    new_nodes[j] = h->nodes[i];
                    ++new_an;
                    --old_an;
                    if (!is_null(h->nodes[i]->val))
                        ++new_n;
                }
            }
        }
        free(h->nodes);
        h->nodes = new_nodes;
        h->n     = new_n;
        h->an    = new_an;
        h->cap   = new_cap;
    }
    int i = node_slot(h->nodes, h->cap, k->hash);
    if (!exists(h, k)) {
        free(h->nodes[i]);
        h->nodes[i] = new_node(k, v);
        h->an++;
        if (set || !is_null(v))
            h->n++;
    } else {
        *h->nodes[i]->val = *v;
    }
    return h->nodes[i]->val;
}

// Remove the key/value pair by nullifying its slot
rf_val *h_delete(rf_htbl *h, rf_str *k) {
    if (!h->nodes || !exists(h, k))
        return NULL;
    if (!k->hash)
        k->hash = u_strhash(k->str);
    int slot = node_slot(h->nodes, h->cap, k->hash);
    rf_val *v = h->nodes[slot]->val;
    h->an--;
    if (!is_null(v))
        h->n--;
    m_freestr(h->nodes[slot]->key);
    h->nodes[slot] = NULL;
    return v;
}
