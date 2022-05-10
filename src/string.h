#ifndef STRING_H
#define STRING_H

#include "types.h"

#define s_eq_raw(x,y) (x->hash == y->hash && x->len == y->len && !memcmp(x->str, y->str, x->len))
#define s_eq(s1,s2) (s1 == s2)
#define s_hash(s) ((s)->hash)
#define s_len(s) ((s)->len)

#define s_coercible(s) ((s)->hash & 0x80000000u)

static inline int s_haszero(rf_str *s) {
    return !!memchr(s->str, '0', s_len(s));
}

void    st_init(void);
rf_str *s_new(const char *, size_t);
rf_str *s_new_concat(char *, char *);
rf_str *s_substr(char *, rf_int, rf_int, rf_int);

#endif
