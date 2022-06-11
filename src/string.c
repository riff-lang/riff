#include "string.h"

#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ST_MIN_CAP 8
#define ST_MAX_LOAD_FACTOR 1.0

typedef struct {
    rf_str   **nodes;
    uint32_t   size;
    uint32_t   mask;
    uint32_t   cap;
    rf_str    *empty;
} rf_stab;

static rf_stab *st = NULL;

void st_init(void) {
    st = malloc(sizeof(rf_stab));
    st->nodes = calloc(ST_MIN_CAP, sizeof(rf_str *));
    st->size  = 0;
    st->mask  = ST_MIN_CAP - 1;
    st->cap   = ST_MIN_CAP;
    rf_str *e = malloc(sizeof(rf_str));
    *e = (rf_str) {0, 0, "", NULL};
    st->empty = e;
}

typedef union {
    rf_hash u;
    uint8_t b[sizeof(rf_hash)];
} str_chunk;

static inline rf_hash chunk(const void *p) {
    return ((const str_chunk *)p)->u;
}

#define coercible_mask(s) (memchr("+-.0123456789", *s, 13) ? 0x80000000u : 0)

static inline rf_hash str_hash(const char *str, size_t len) {
    rf_hash a, b, h = len;
    if (len >= 4) {
        a  = chunk(str);
        h ^= chunk(str+len-4);
        b  = chunk(str+(len>>1)-2);
        h ^= b;
        h -= rol(b,14);
        b += chunk(str+(len>>2)-1);
    } else {
        a  = *(const uint8_t *)str;
        h ^= *(const uint8_t *)(str+len-1);
        b  = *(const uint8_t *)(str+(len>>1));
        h ^= b;
        h -= rol(b,14);
    }
    a ^= h; a -= rol(h, 11);
    b ^= a; b -= rol(a, 25);
    h ^= b; h -= rol(b, 16);

    h &= ~0x80000000u; // clear MSB
    return h | coercible_mask(str);
}

static inline rf_str *next(rf_str *s) {
    return s->next;
}

static inline double potential_lf(rf_stab *t) {
    double n1 = (double) t->size + 1.0;
    double n2 = (double) t->cap;
    return n1 / n2;
}

static inline void insert_str(rf_str **nodes, rf_str *new, uint32_t i) {
    rf_str *n = nodes[i];
    if (!n) {
        nodes[i] = new;
        return;
    }
    while (n->next)
        n = next(n);
    n->next = new;
}

static inline rf_str *new_str(rf_str *s) {
    size_t len = s_len(s);
    rf_str *new = malloc(sizeof(rf_str));
    char *str = malloc(len * sizeof(char) + 1);
    memcpy(str, s->str, len);
    str[len] = '\0';
    *new = (rf_str) {s->hash, len, str, NULL};
    return new;
}

static inline void st_resize(rf_stab *t, size_t new_cap) {
    rf_str **new_nodes = calloc(new_cap, sizeof(rf_str *));
    for (uint32_t i = 0; i < t->cap; ++i) {
        rf_str *s = t->nodes[i];
        while (s) {
            insert_str(new_nodes, s, s->hash & (new_cap-1));
            rf_str *p = s;
            s = next(s);
            p->next = NULL;
        }
    }
    free(t->nodes);
    t->nodes = new_nodes;
    t->mask = new_cap - 1;
    t->cap = new_cap;
}

static inline rf_str *st_lookup(rf_stab *t, rf_str *s) {
    rf_str *n = t->nodes[s->hash & t->mask];
    while (n) {
        if (s_eq_raw(n,s))
            return n;
        n = next(n);
    }
    if (potential_lf(t) > ST_MAX_LOAD_FACTOR)
        st_resize(t, t->cap << 1);
    rf_str *new = new_str(s);
    insert_str(t->nodes, new, s->hash & t->mask);
    t->size++;
    return new;
}

// Create string (does not assume null-terminated input)
rf_str *s_new(const char *start, size_t len) {
    return len ? st_lookup(st, &(rf_str){str_hash(start,len),len,(char *)start,NULL}) : st->empty;
}

// Assumes null-terminated strings
rf_str *s_new_concat(char *l, char *r) {
    size_t l_len = strlen(l);
    size_t r_len = strlen(r);
    size_t new_len = l_len + r_len;
    char *new = malloc(new_len * sizeof(char) + 1);
    memcpy(new, l, l_len);
    memcpy(new + l_len, r, r_len);
    return s_new(new, new_len);
}

rf_str *s_substr(char *s, rf_int from, rf_int to, rf_int itvl) {
    // Correct out-of-bounds ranges
    size_t sl = strlen(s) - 1;
    from = from > sl ? sl : from < 0 ? 0 : from;
    to   = to   > sl ? sl : to   < 0 ? 0 : to;

    size_t len;
    if (itvl > 0)
        len = (size_t) (to - from) + 1;
    else
        len = (size_t) (from - to) + 1;
    // Disallow contradicting directions. E.g. negative intervals when
    // from > to. Infer the direction from the from/to range and
    // override the provided interval with its negative value.
    if ((from < to && itvl < 1) ||
        (from > to && itvl > -1))
        itvl = -itvl;

    len = (size_t) ceil(fabs((double) len / (double) itvl));
    char *str = malloc(len * sizeof(char));
    for (size_t i = 0; i < len; ++i) {
        str[i] = s[from];
        from += itvl;
    }
    return s_new(str, len);
}
