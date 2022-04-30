#include "table.h"

#include "mem.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define T_MIN_LOAD_FACTOR  0.5

#define HT_MIN_CAP         8
#define HT_MIN_LOAD_FACTOR 0.4
#define HT_MAX_LOAD_FACTOR 1.0

static inline ht_node *next(ht_node *);
static uint32_t ht_logical_size(rf_htab *);
static inline rf_val *ht_delete_val(rf_htab *, rf_val *);

void t_init(rf_tab *t) {
    t->nullx = 0;
    t->hint  = 0;
    t->lsize = 0;
    t->psize = 0;
    t->cap   = 0;
    t->nullv = v_newnull();
    t->v     = NULL;
    t->h     = malloc(sizeof(rf_htab));
    ht_init(t->h);
}

static rf_val *reduce_key(rf_val *s, rf_val *d) {
    switch (s->type) {
    case TYPE_FLT:
        if (s->u.f == ceil(s->u.f)) {
            set_int(d, (rf_int) s->u.f);
            return d;
        }
        break;
    case TYPE_STR: {
        if (s_numunlikely(s->u.s))
            return s;
        char *end;
        rf_int i = u_str2i64(s->u.s->str, &end, 0);
        if (!*end) {
            set_int(d, i);
            if (!i)
                return s_haszero(s->u.s) ? d : s;
            return d;
        }
        rf_flt f = u_str2d(s->u.s->str, &end, 0);
        if (!*end) {
            set_flt(d, f);
            if (f == 0.0) {
                if (s_haszero(s->u.s)) {
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

rf_int t_logical_size(rf_tab *t) {
    if (!t->hint)
        return t->lsize + ht_logical_size(t->h);
    rf_int l = 0;
    for (int i = 0; i < t->cap; ++i) {
        if (t->v[i] && !is_null(t->v[i]))
            ++l;
    }
    // Include special "null" index
    l += (t->nullx && !is_null(t->nullv));
    t->lsize = l;
    t->hint = 0;
    return l + ht_logical_size(t->h);
}

// Don't call if k < 0
static int t_exists(rf_tab *t, rf_int k) {
    return k < t->cap && t->v[k] != NULL;
}

static inline void ht_collect_keys(rf_htab *h, rf_val *keys, int *n) {
    for (uint32_t i = 0; i < h->cap; ++i) {
        ht_node *node = h->nodes[i];
        while (node) {
            if (!is_null(node->v))
                keys[*n++] = (rf_val) {node->k.val->type, node->k.val->u};
            node = next(node);
        }
    }
}

// NOTE: The language currently defines the order of iteration as:
// - Integer keys 0..N
// - Misc hash table keys (no particular order)
// - null key
rf_val *t_collect_keys(rf_tab *t) {
    uint32_t len = t_logical_size(t);
    rf_val *keys = malloc(len * sizeof(rf_val));
    int n = 0;
    for (uint32_t i = 0; i < t->cap && n <= len; ++i) {
        if (t_exists(t,i) && !is_null(t->v[i])) {
            keys[n++] = (rf_val) {TYPE_INT, .u.i = i};
        }
    }
    // TODO pass `len`, allowing function to exit early if possible
    ht_collect_keys(t->h, keys, &n);
    if (t->nullx)
        keys[n++] = (rf_val) {TYPE_NULL, .u.i = 0};
    return keys;
}

static double t_potential_lf(uint32_t n, uint32_t cap, rf_int k) {
    double n1 = n + 1.0;
    double n2 = cap > k ? cap : k + 1.0;
    return n1 / n2;
}

static inline int would_fit(rf_tab *t, rf_int k) {
    return k >= 0 &&
        (k < t->cap || t_potential_lf(t->psize, t->cap, k) >= T_MIN_LOAD_FACTOR);
}

rf_val *t_lookup(rf_tab *t, rf_val *k, int hint) {
    if (hint) t->hint = 1;
    rf_val tmp;
    k = reduce_key(k, &tmp);
    switch (k->type) {
    case TYPE_NULL:
        if (hint) t->nullx = 1;
        return t->nullv;
    case TYPE_INT:
        if (k->u.i >= 0) {
            rf_int ki = k->u.i;
            if (t_exists(t, ki))
                return t->v[ki];
            if (would_fit(t, ki))
                return t_insert_int(t, ki, NULL);
        }
        // Fall-through
    default: return ht_lookup_val(t->h, k);
    }
}

static uint32_t new_size(uint32_t n, uint32_t cap, rf_int k) {
    uint32_t sz = cap < k ? k + 1 : cap + 1;
    return (uint32_t) (sz / T_MIN_LOAD_FACTOR) + 1;
}

// Don't call with k < 0
rf_val *t_insert_int(rf_tab *t, rf_int k, rf_val *v) {
    if (k >= t->cap) {
        uint32_t old_cap = t->cap;
        uint32_t new_cap = new_size(t->psize, old_cap, k);
        t->v = realloc(t->v, new_cap * sizeof(rf_val *));
        for (uint32_t i = old_cap; i < new_cap; ++i) {
            if (t_exists(t,i)) continue;
            t->v[i] = ht_delete_val(t->h, &(rf_val){TYPE_INT, .u.i = i});
            if (t->v[i])
                t->psize++;
        }
        t->cap = new_cap;
    }
    if (!t_exists(t,k)) {
        t->v[k] = v == NULL ? v_newnull() : v_copy(v);
        t->psize++;
    } else if (v != NULL) {
        *t->v[k] = *v;
    }
    t->hint = 1;
    return t->v[k];
}

// Hash tables

void ht_init(rf_htab *h) {
    h->nodes = NULL;
    h->lsize = 0;
    h->psize = 0;
    h->cap   = 0;
    h->hint  = 0;
}

static inline ht_node *next(ht_node *n) {
    return n->next;
}

static uint32_t ht_logical_size(rf_htab *h) {
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

static inline double ht_potential_lf(rf_htab *h) {
    double n1 = (double) h->psize + 1.0;
    double n2 = (double) h->cap;
    return n1 / n2;
}

// Ripped from LuaJIT
#define rol(x,n) (((x)<<(n)) | ((x)>>(-(int)(n)&(8*sizeof(x)-1))))
static inline uint32_t hashrot(uint32_t lo, uint32_t hi) {
    lo ^= hi; hi  = rol(hi, 14);
    lo -= hi; hi  = rol(hi, 5);
    hi ^= lo; hi -= rol(lo, 13);
    return hi;
}

static inline uint32_t anchor(rf_val *k, uint32_t mask) {
    switch (k->type) {
    case TYPE_INT: return ((uint32_t) k->u.i) & mask;
    case TYPE_STR: return s_hash(k->u.s) & mask;
    default:
        return hashrot(
            (uint32_t) (k->u.i & UINT32_MAX),
            (uint32_t) ((k->u.i >> 32) & UINT32_MAX)
        ) & mask;
    }
}

static inline void insert_node(ht_node **nodes, ht_node *new, uint32_t i) {
    ht_node *n = nodes[i];
    if (!n) {
        nodes[i] = new;
        return;
    }
    while (n->next)
        n = next(n);
    n->next = new;
}

static inline void ht_resize_val(rf_htab *h, size_t new_cap) {
    ht_node **new_nodes = malloc(sizeof(ht_node *) * new_cap);
    for (uint32_t i = 0; i < new_cap; ++i)
        new_nodes[i] = NULL;
    for (uint32_t i = 0; i < h->cap; ++i) {
        ht_node *n = h->nodes[i];
        while (n) {
            insert_node(new_nodes, n, anchor(n->k.val, new_cap-1));
            ht_node *p = n;
            n = next(n);
            p->next = NULL;
        }
    }
    free(h->nodes);
    h->nodes = new_nodes;
    h->cap = new_cap;
}

static inline void ht_resize_str(rf_htab *h, size_t new_cap) {
    ht_node **new_nodes = malloc(sizeof(ht_node *) * new_cap);
    for (uint32_t i = 0; i < new_cap; ++i)
        new_nodes[i] = NULL;
    for (uint32_t i = 0; i < h->cap; ++i) {
        ht_node *n = h->nodes[i];
        while (n) {
            insert_node(new_nodes, n, n->k.str->hash & (new_cap-1));
            ht_node *p = n;
            n = next(n);
            p->next = NULL;
        }
    }
    free(h->nodes);
    h->nodes = new_nodes;
    h->cap = new_cap;
}

#define node_eq_str(s1, s2) (s_eq_fast(s1, s2))

static inline int node_eq_val(rf_val *v1, rf_val *v2) {
    if (v1->type != v2->type)
        return 0;
    switch (v1->type) {
    case TYPE_FLT: return v1->u.f == v2->u.f;
    case TYPE_STR: return node_eq_str(v1->u.s, v2->u.s);
    default: return v1->u.i == v2->u.i;
    }
}

#define HT_LOOKUP(type, mask) \
    h->hint = 1; \
    if (h->nodes == NULL) \
        return ht_insert_##type(h, k, NULL); \
    ht_node *n = h->nodes[(mask)]; \
    while (n) { \
        if (node_eq_##type(n->k.type, k)) \
            return n->v; \
        n = next(n); \
    } \
    return ht_insert_##type(h, k, NULL);

rf_val *ht_lookup_val(rf_htab *h, rf_val *k) {
    HT_LOOKUP(val, anchor(k, h->cap-1))
}

rf_val *ht_lookup_str(rf_htab *h, rf_str *k) {
    HT_LOOKUP(str, s_hash(k) & (h->cap-1))
}

static inline ht_node *new_node_val(rf_val *k, rf_val *v) {
    ht_node *new = malloc(sizeof(ht_node));
    *new = (ht_node) {
        .k.val = v_copy(k),
        v == NULL ? v_newnull() : v_copy(v),
        NULL
    };
    return new;
}

static inline ht_node *new_node_str(rf_str *k, rf_val *v) {
    ht_node *new = malloc(sizeof(ht_node));
    *new = (ht_node) {
        .k.str = k, // TODO ensure stack-allocated (temp) strings aren't an issue
        v == NULL ? v_newnull() : v_copy(v),
        NULL
    };
    return new;
}

#define HT_INSERT(type, mask) \
    if (h->nodes == NULL) { \
        h->nodes = malloc(sizeof(ht_node *) * HT_MIN_CAP); \
        h->cap = HT_MIN_CAP; \
        for (int i = 0; i < HT_MIN_CAP; ++i) \
            h->nodes[i] = NULL; \
    } else { \
        double lf = ht_potential_lf(h); \
        if (lf > HT_MAX_LOAD_FACTOR) \
            ht_resize_##type(h, h->cap << 1); \
        else if (h->cap > HT_MIN_CAP && lf < HT_MIN_LOAD_FACTOR) \
            ht_resize_##type(h, h->cap >> 1); \
    } \
    ht_node *new = new_node_##type(k,v); \
    insert_node(h->nodes, new, (mask)); \
    h->psize++; \
    return new->v;

rf_val *ht_insert_val(rf_htab *h, rf_val *k, rf_val *v) {
    HT_INSERT(val, anchor(k, h->cap-1))
}

rf_val *ht_insert_str(rf_htab *h, rf_str *k, rf_val *v) {
    HT_INSERT(str, s_hash(k) & (h->cap-1))
}

rf_val *ht_insert_cstr(rf_htab *h, const char *k, rf_val *v) {
    return ht_insert_str(h, s_newstr(k, strlen(k), 1), v);
}

static inline rf_val *ht_delete_val(rf_htab *h, rf_val *k) {
    if (h->nodes == NULL)
        return NULL;
    ht_node **a = &h->nodes[anchor(k, h->cap-1)];
    ht_node *n = *a;
    while (n) {
        if (node_eq_val(n->k.type, k)) {
            *a = n->next;
            rf_val *v = n->v;
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
