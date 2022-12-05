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
    *e = (riff_str) {
        .hash  = 0,
        .hints = 0,
        .extra = 0,
        .len   = 0,
        .str   = "",
        .next  = NULL
    };
    st->empty = e;
}

static inline strhash chunk(const void *p) {
    return *(strhash *) p;
}

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
    return h;
}

#define has_zero(s,l)       (!!memchr(s, '0', l))
#define likely_coercible(s) (riff_strchr("+-.0123456789", *s))

// NOTE: existence of '0' does not imply coercibility
static inline uint8_t str_hints(const char *str, size_t len) {
    uint8_t flags = 0;
    if (has_zero(str, len))    flags |= RIFF_STR_HINT_ZERO;
    if (likely_coercible(str)) flags |= RIFF_STR_HINT_COERCIBLE;
    return flags;
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
    if (riff_unlikely(!n)) {
        nodes[i] = new;
        return;
    }
    while (n->next) {
        n = next(n);
    }
    n->next = new;
}

static inline riff_str *new_str(riff_str *s) {
    size_t len = riff_strlen(s);
    riff_str *new = malloc(sizeof(riff_str));
    char *str = malloc(len * sizeof(char) + 1);
    memcpy(str, s->str, len);
    str[len] = '\0';
    memcpy(new, s, sizeof(riff_str));
    new->str = str;
    new->next = NULL;
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
        if (riff_likely(riff_str_eq_raw(n,s))) {
            return n;
        }
        n = next(n);
    }
    if (riff_unlikely(potential_lf(t) > ST_MAX_LOAD_FACTOR)) {
        st_resize(t, t->cap << 1);
    }
    riff_str *new = new_str(s);
    insert_str(t->nodes, new, s->hash & t->mask);
    t->size++;
    return new;
}

riff_str *riff_str_new_extra(const char *start, size_t len, uint8_t extra) {
    return riff_likely(len)
        ? st_lookup(st,
            &(riff_str) {
                .hash  = str_hash(start, len),
                .hints = str_hints(start, len),
                .extra = extra,
                .len   = len,
                .str   = (char *) start,
                .next  = NULL
            }
          )
        : st->empty;
}

// Create string (does not assume null-terminated input)
riff_str *riff_str_new(const char *start, size_t len) {
    return riff_str_new_extra(start, len, 0);
}

riff_str *riff_strcat(char *l, char *r, size_t llen, size_t rlen) {
    size_t len = llen + rlen;
    char new[len];
    memcpy(new, l, llen);
    memcpy(new + llen, r, rlen);
    return riff_str_new(new, len);
}

riff_str *riff_substr(char *s, riff_int from, riff_int to, riff_int itvl) {
    // Correct out-of-bounds ranges
    size_t sl = strlen(s);
    if (!sl)
        return riff_str_new("", 0);
    --sl;
    from = from > sl ? sl : from < 0 ? 0 : from;
    to   = to   > sl ? sl : to   < 0 ? 0 : to;

    size_t len;
    if (itvl > 0) {
        len = (size_t) (to - from) + 1;
    } else {
        len = (size_t) (from - to) + 1;
    }
    // Disallow contradicting directions. E.g. negative intervals when
    // from > to. Infer the direction from the from/to range and
    // override the provided interval with its negative value.
    if ((from < to && itvl < 1) ||
        (from > to && itvl > -1)) {
        itvl = -itvl;
    }
    len = (size_t) ceil(fabs((double) len / (double) itvl));
    char str[len];
    for (size_t i = 0; i < len; ++i) {
        str[i] = s[from];
        from += itvl;
    }
    return riff_str_new(str, len);
}
