#ifndef STRING_H
#define STRING_H

#include "types.h"

#define s_len(s) ((s)->len)

// djb2
// source: http://www.cse.yorku.ca/~oz/hash.html
static inline rf_hash u_strhash(const char *str) {
    rf_hash h = 5381;
    int c;
    while ((c = *str++))
        h = ((h << 5) + h) + c;
    return h;
}

static inline rf_hash s_hash(rf_str *s) {
    return s->hash ? s->hash : (s->hash = u_strhash(s->str));
}

static inline int s_eq_fast(rf_str *s1, rf_str *s2) {
    return s_hash(s1) == s_hash(s2);
}

static inline int s_numunlikely(rf_str *s) {
    return !s_len(s) || !memchr("+-.0123456789", s->str[0], 13);
}

static inline int s_haszero(rf_str *s) {
    return !!memchr(s->str, '0', s_len(s));
}

rf_str *s_new(const char *, size_t);
rf_str *s_newh(const char *, size_t);
rf_str *s_new_concat(char *, char *);
rf_str *s_substr(char *, rf_int, rf_int, rf_int);

#endif
