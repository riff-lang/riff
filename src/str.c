#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

// djb2; source: http://www.cse.yorku.ca/~oz/hash.html
uint32_t s_hash(const char *str) {
    uint32_t h = 5381;
    unsigned int c;
    while ((c = *str++))
        h = ((h << 5) + h) + c;
    return h;
}

str_t *s_newstr(const char *start, size_t l, int h) {
    char *str = malloc(l * sizeof(char) + 1);
    memcpy(str, start, l);
    str[l] = '\0';
    str_t *s = malloc(sizeof(str_t) + 1);
    s->l = l;
    s->hash = h ? s_hash(str) : 0;
    s->str = str;
    return s;
}

str_t *s_concat(str_t *l, str_t *r, int h) {
    size_t nl = l->l + r->l;
    char *new = malloc(nl * sizeof(char) + 1);
    memcpy(new, l->str, l->l);
    memcpy(new + l->l, r->str, r->l);
    new[nl] = '\0';
    str_t *s = malloc(sizeof(str_t) + 1);
    s->l = nl;
    s->hash = h ? s_hash(new) : 0;
    s->str = new;
    return s;
}

str_t *s_int2str(int_t i) {
    int len = snprintf(NULL, 0, "%lld", i);
    char buf[len + 1];
    snprintf(buf, len + 1, "%lld", i);
    return s_newstr(buf, len, 0);
}

str_t *s_flt2str(flt_t f) {
    int len = snprintf(NULL, 0, "%g", f);
    char buf[len + 1];
    snprintf(buf, len + 1, "%g", f);
    return s_newstr(buf, len, 0);
}
