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
    riff_str **nodes;
    uint32_t   size;
    uint32_t   mask;
    uint32_t   cap;
    riff_str  *empty;
} riff_stab;

static riff_stab *st = NULL;

void riff_stab_init(void) {
    st = malloc(sizeof(riff_stab));
    st->nodes = calloc(ST_MIN_CAP, sizeof(riff_str *));
    st->size  = 0;
    st->mask  = ST_MIN_CAP - 1;
    st->cap   = ST_MIN_CAP;
    riff_str *e = malloc(sizeof(riff_str));
    *e = (riff_str) {0, 0, "", NULL};
    st->empty = e;
}

static inline strhash chunk(const void *p) {
    return *(strhash *) p;
}

#define coercible_mask(s) (memchr("+-.0123456789", *s, 13) ? 0x80000000u : 0)

static inline strhash str_hash(const char *str, size_t len) {
    strhash a, b, h = len;
    if (len >= sizeof(strhash)) {
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

static inline riff_str *next(riff_str *s) {
    return s->next;
}

static inline double potential_lf(riff_stab *t) {
    double n1 = (double) t->size + 1.0;
    double n2 = (double) t->cap;
    return n1 / n2;
}

static inline void insert_str(riff_str **nodes, riff_str *new, uint32_t i) {
    riff_str *n = nodes[i];
    if (UNLIKELY(!n)) {
        nodes[i] = new;
        return;
    }
    while (n->next)
        n = next(n);
    n->next = new;
}

static inline riff_str *new_str(riff_str *s) {
    size_t len = s_len(s);
    riff_str *new = malloc(sizeof(riff_str));
    char *str = malloc(len * sizeof(char) + 1);
    memcpy(str, s->str, len);
    str[len] = '\0';
    *new = (riff_str) {s->hash, len, str, NULL};
    return new;
}

static inline void st_resize(riff_stab *t, size_t new_cap) {
    riff_str **new_nodes = calloc(new_cap, sizeof(riff_str *));
    for (uint32_t i = 0; i < t->cap; ++i) {
        riff_str *s = t->nodes[i];
        while (s) {
            insert_str(new_nodes, s, s->hash & (new_cap-1));
            riff_str *p = s;
            s = next(s);
            p->next = NULL;
        }
    }
    free(t->nodes);
    t->nodes = new_nodes;
    t->mask = new_cap - 1;
    t->cap = new_cap;
}

static inline riff_str *st_lookup(riff_stab *t, riff_str *s) {
    riff_str *n = t->nodes[s->hash & t->mask];
    while (n) {
        if (LIKELY(s_eq_raw(n,s)))
            return n;
        n = next(n);
    }
    if (UNLIKELY(potential_lf(t) > ST_MAX_LOAD_FACTOR))
        st_resize(t, t->cap << 1);
    riff_str *new = new_str(s);
    insert_str(t->nodes, new, s->hash & t->mask);
    t->size++;
    return new;
}

// Create string (does not assume null-terminated input)
riff_str *s_new(const char *start, size_t len) {
    return LIKELY(len) ? st_lookup(st, &(riff_str){str_hash(start,len),len,(char *)start,NULL}) : st->empty;
}

// Assumes null-terminated strings
riff_str *s_new_concat(char *l, char *r) {
    size_t l_len = strlen(l);
    size_t r_len = strlen(r);
    size_t new_len = l_len + r_len;
    char *new = malloc(new_len * sizeof(char) + 1);
    memcpy(new, l, l_len);
    memcpy(new + l_len, r, r_len);
    return s_new(new, new_len);
}

riff_str *s_substr(char *s, riff_int from, riff_int to, riff_int itvl) {
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
