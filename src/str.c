#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

#define copystr(d,s,n) \
    d = malloc(n * sizeof(char)); \
    memcpy(d, s, n * sizeof(char)); \
    d[n] = '\0';

// djb2; source: http://www.cse.yorku.ca/~oz/hash.html
static uint32_t s_hash(const char *str) {
    uint32_t h = 5381;
    unsigned int c;
    while ((c = *str++))
        h = ((h << 5) + h) + c;
    return h;
}

// TODO These strings functions are likely extremely slow and wasteful
// of memory

str_t *s_newstr(const char *start, size_t l) {
    char *str;
    copystr(str, start, l);
    str_t *s = malloc(sizeof(str_t));
    s->l = l;
    s->hash = s_hash(str);
    s->str = str;
    return s;
}

str_t *s_concat(str_t *l, str_t *r) {
    char *new;
    copystr(new, l->str, l->l);
    strncat(new, r->str, l->l + r->l);
    return s_newstr(new, l->l + r->l);
}

str_t *s_int2str(int_t i) {
    int len = snprintf(NULL, 0, "%lld", i);
    char buf[len + 1];
    snprintf(buf, len + 1, "%lld", i);
    return s_newstr(buf, len);
}

str_t *s_flt2str(flt_t f) {
    int len = snprintf(NULL, 0, "%g", f);
    char buf[len + 1];
    snprintf(buf, len + 1, "%g", f);
    return s_newstr(buf, len);
}

#undef copystr
