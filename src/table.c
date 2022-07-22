#include "table.h"

#include "mem.h"
#include "string.h"
#include "util.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define T_MIN_LOAD_FACTOR  0.5

#define HT_MIN_CAP         8
#define HT_MIN_LOAD_FACTOR 0.4
#define HT_MAX_LOAD_FACTOR 1.0

static inline ht_node  *next(ht_node *);
static uint32_t         riff_htab_logical_size(riff_htab *);
static inline riff_val *riff_htab_delete_val(riff_htab *, riff_val *);

void riff_tab_init(riff_tab *t) {
    t->nullx = 0;
    t->hint  = 0;
    t->lsize = 0;
    t->psize = 0;
    t->cap   = 0;
    t->nullv = v_newnull();
    t->v     = NULL;
    t->h     = malloc(sizeof(riff_htab));
    riff_htab_init(t->h);
}

static riff_val *reduce_key(riff_val *s, riff_val *d) {
    switch (s->type) {
    case TYPE_FLT:
        if (UNLIKELY(s->f == ceil(s->f))) {
            set_int(d, (riff_int) s->f);
            return d;
        }
        break;
    case TYPE_STR: {
        if (LIKELY(!s_coercible(s->s)))
            return s;
        char *end;
        riff_int i = u_str2i64(s->s->str, &end, 0);
        if (!*end) {
            set_int(d, i);
            if (!i)
                return s_haszero(s->s) ? d : s;
            return d;
        }
        riff_float f = u_str2d(s->s->str, &end, 0);
        if (!*end) {
            set_flt(d, f);
            if (f == 0.0) {
                if (s_haszero(s->s)) {
                    set_int(d, 0);
                    return d;
                } else {
                    return s;
                }
            }
            return d;
        }
        break;
    }
    }
    return s;
}

riff_int riff_tab_logical_size(riff_tab *t) {
    if (LIKELY(!t->hint))
        return t->lsize + riff_htab_logical_size(t->h);
    riff_int l = 0;
    for (int i = 0; i < t->cap; ++i) {
        if (t->v[i] && !is_null(t->v[i]))
            ++l;
    }
    // Include special "null" index
    l += (t->nullx && !is_null(t->nullv));
    t->lsize = l;
    t->hint = 0;
    return l + riff_htab_logical_size(t->h);
}

// Don't call if k < 0
static int t_exists(riff_tab *t, riff_int k) {
    return k < t->cap && t->v[k] != NULL;
}

static inline void riff_htab_collect_keys(riff_htab *h, riff_val *keys, int *n) {
    for (uint32_t i = 0; i < h->cap; ++i) {
        ht_node *node = h->nodes[i];
        while (node) {
            if (LIKELY(!is_null(node->v)))
                keys[(*n)++] = (riff_val) {node->k.val->type, node->k.val->i};
            node = next(node);
        }
    }
}

// NOTE: The language currently defines the order of iteration as:
// - Integer keys 0..N
// - Hash table keys (order not guaranteed)
// - null key
riff_val *riff_tab_collect_keys(riff_tab *t) {
    uint32_t len = riff_tab_logical_size(t);
    riff_val *keys = malloc(len * sizeof(riff_val));
    int n = 0;
    for (uint32_t i = 0; i < t->cap && n <= len; ++i) {
        if (t_exists(t,i) && !is_null(t->v[i])) {
            keys[n++] = (riff_val) {TYPE_INT, .i = i};
        }
    }
    // TODO pass `len`, allowing function to exit early if possible
    riff_htab_collect_keys(t->h, keys, &n);
    if (UNLIKELY(t->nullx))
        keys[n++] = (riff_val) {TYPE_NULL, .i = 0};
    return keys;
}

static double t_potential_lf(uint32_t n, uint32_t cap, riff_int k) {
    double n1 = n + 1.0;
    double n2 = cap > k ? cap : k + 1.0;
    return n1 / n2;
}

static inline int would_fit(riff_tab *t, riff_int k) {
    return k >= 0 &&
        (k < t->cap || t_potential_lf(t->psize, t->cap, k) >= T_MIN_LOAD_FACTOR);
}

riff_val *riff_tab_lookup(riff_tab *t, riff_val *k, int hint) {
    if (hint) t->hint = 1;
    riff_val tmp;
    k = reduce_key(k, &tmp);
    switch (k->type) {
    case TYPE_NULL:
        if (hint) t->nullx = 1;
        return t->nullv;
    case TYPE_INT:
        if (k->i >= 0) {
            riff_int ki = k->i;
            if (t_exists(t, ki))
                return t->v[ki];
            if (would_fit(t, ki))
                return riff_tab_insert_int(t, ki, NULL);
        }
        // Fall-through
    default: return riff_htab_lookup_val(t->h, k);
    }
}

static uint32_t new_size(uint32_t n, uint32_t cap, riff_int k) {
    uint32_t sz = cap < k ? k + 1 : cap + 1;
    return (uint32_t) (sz / T_MIN_LOAD_FACTOR) + 1;
}

// Don't call with k < 0
riff_val *riff_tab_insert_int(riff_tab *t, riff_int k, riff_val *v) {
    if (k >= t->cap) {
        uint32_t old_cap = t->cap;
        uint32_t new_cap = new_size(t->psize, old_cap, k);
        t->v = realloc(t->v, new_cap * sizeof(riff_val *));
        for (uint32_t i = old_cap; i < new_cap; ++i) {
            if (t_exists(t,i)) continue;
            t->v[i] = riff_htab_delete_val(t->h, &(riff_val){TYPE_INT, .i = i});
            if (t->v[i])
                t->psize++;
        }
        t->cap = new_cap;
    }
    if (LIKELY(!t_exists(t,k))) {
        t->v[k] = v == NULL ? v_newnull() : v_copy(v);
        t->psize++;
    } else if (LIKELY(v != NULL)) {
        *t->v[k] = *v;
    }
    t->hint = 1;
    return t->v[k];
}

// Hash tables

void riff_htab_init(riff_htab *h) {
    h->nodes = NULL;
    h->lsize = 0;
    h->psize = 0;
    h->mask  = 0;
    h->cap   = 0;
    h->hint  = 0;
}

static inline ht_node *next(ht_node *n) {
    return n->next;
}

static uint32_t riff_htab_logical_size(riff_htab *h) {
    if (!h->hint)
        return h->lsize;
    uint32_t l = 0;
    for (uint32_t i = 0; i < h->cap; ++i) {
        ht_node *n = h->nodes[i];
        while (n) {
            if (!is_null(n->v))
                ++l;
            n = next(n);
        }
    }
    h->hint = 0;
    return (h->lsize = l);
}

static inline double ht_potential_lf(riff_htab *h) {
    double n1 = (double) h->psize + 1.0;
    double n2 = (double) h->cap;
    return n1 / n2;
}

static inline uint32_t hashrot(uint32_t lo, uint32_t hi) {
    lo ^= hi; hi  = rol(hi, 14);
    lo -= hi; hi  = rol(hi, 5);
    hi ^= lo; hi -= rol(lo, 13);
    return hi;
}

static inline uint32_t anchor(riff_val *k, uint32_t mask) {
    switch (k->type) {
    case TYPE_INT: return ((uint32_t) k->i) & mask;
    case TYPE_STR: return s_hash(k->s) & mask;
    default:
        return hashrot(
            (uint32_t) (k->i & UINT32_MAX),
            (uint32_t) ((k->i >> 32) & UINT32_MAX)
        ) & mask;
    }
}

static inline void insert_node(ht_node **nodes, ht_node *new, uint32_t i) {
    ht_node *n = nodes[i];
    if (UNLIKELY(!n)) {
        nodes[i] = new;
        return;
    }
    while (n->next) {
        n = next(n);
    }
    n->next = new;
}

#define HT_RESIZE(mask_type) \
    ht_node **new_nodes = calloc(new_cap, sizeof(ht_node *)); \
    for (uint32_t i = 0; i < h->cap; ++i) { \
        ht_node *n = h->nodes[i]; \
        while (n) { \
            insert_node(new_nodes, n, (mask_type)); \
            ht_node *p = n; \
            n = next(n); \
            p->next = NULL; \
        } \
    } \
    free(h->nodes); \
    h->nodes = new_nodes; \
    h->mask = new_cap - 1; \
    h->cap = new_cap;

static inline void ht_resize_val(riff_htab *h, size_t new_cap) {
    HT_RESIZE(anchor(n->k.val, new_cap-1))
}

static inline void ht_resize_str(riff_htab *h, size_t new_cap) {
    HT_RESIZE(n->k.str->hash & (new_cap-1))
}

#define node_eq_str(s1, s2) (s_eq(s1, s2))

static inline int node_eq_val(riff_val *v1, riff_val *v2) {
    if (UNLIKELY(v1->type != v2->type))
        return 0;
    switch (v1->type) {
    case TYPE_FLT: return v1->f == v2->f;
    case TYPE_STR: return node_eq_str(v1->s, v2->s);
    default: return v1->i == v2->i;
    }
}

#define HT_LOOKUP(type, mask) \
    if (UNLIKELY(h->nodes == NULL)) \
        return riff_htab_insert_##type(h, k, NULL); \
    ht_node *n = h->nodes[(mask)]; \
    while (n) { \
        if (UNLIKELY(node_eq_##type(n->k.type, k))) \
            return n->v; \
        n = next(n); \
    } \
    return riff_htab_insert_##type(h, k, NULL);

riff_val *riff_htab_lookup_val(riff_htab *h, riff_val *k) {
    h->hint = 1;
    HT_LOOKUP(val, anchor(k, h->mask))
}

riff_val *riff_htab_lookup_str(riff_htab *h, riff_str *k) {
    HT_LOOKUP(str, s_hash(k) & (h->mask))
}

static inline ht_node *new_node_val(riff_val *k, riff_val *v) {
    ht_node *new = malloc(sizeof(ht_node));
    *new = (ht_node) {
        .k.val = v_copy(k),
        v == NULL ? v_newnull() : v_copy(v),
        NULL
    };
    return new;
}

static inline ht_node *new_node_str(riff_str *k, riff_val *v) {
    ht_node *new = malloc(sizeof(ht_node));
    *new = (ht_node) {
        .k.str = k,
        v == NULL ? v_newnull() : v_copy(v),
        NULL
    };
    return new;
}

#define HT_INSERT(type, mask_type) \
    if (UNLIKELY(h->nodes == NULL)) { \
        h->nodes = calloc(HT_MIN_CAP, sizeof(ht_node *)); \
        h->mask = HT_MIN_CAP - 1; \
        h->cap = HT_MIN_CAP; \
    } else { \
        double lf = ht_potential_lf(h); \
        if (lf > HT_MAX_LOAD_FACTOR) \
            ht_resize_##type(h, h->cap << 1); \
        else if (UNLIKELY(h->cap > HT_MIN_CAP && lf < HT_MIN_LOAD_FACTOR)) \
            ht_resize_##type(h, h->cap >> 1); \
    } \
    ht_node *new = new_node_##type(k,v); \
    insert_node(h->nodes, new, (mask_type)); \
    h->psize++; \
    return new->v;

riff_val *riff_htab_insert_val(riff_htab *h, riff_val *k, riff_val *v) {
    HT_INSERT(val, anchor(k, h->mask))
}

riff_val *riff_htab_insert_str(riff_htab *h, riff_str *k, riff_val *v) {
    HT_INSERT(str, s_hash(k) & (h->mask))
}

riff_val *riff_htab_insert_cstr(riff_htab *h, const char *k, riff_val *v) {
    return riff_htab_insert_str(h, s_new(k, strlen(k)), v);
}

static inline riff_val *riff_htab_delete_val(riff_htab *h, riff_val *k) {
    if (UNLIKELY(h->nodes == NULL))
        return NULL;
    ht_node **a = &h->nodes[anchor(k, h->mask)];
    ht_node *n = *a;
    while (n) {
        if (node_eq_val(n->k.val, k)) {
            *a = n->next;
            riff_val *v = n->v;
            free(n->k.val);
            free(n);
            h->psize--;
            return v;
        }
        a = &n->next;
        n = next(n);
    }
    return NULL;
}
