#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

// djb2; source: http://www.cse.yorku.ca/~oz/hash.html
static uint32_t s_hash(const char *str) {
    uint32_t h = 5381;
    unsigned int c;
    while ((c = *str++))
        h = ((h << 5) + h) + c;
    return h;
}

str_t *s_newstr(const char *start, size_t l) {
    char *str;
    str = malloc(l * sizeof(char) + 1);
    memcpy(str, start, l * sizeof(char));
    str[l] = '\0';
    str_t *s = NULL;
    s = malloc(sizeof(str_t) + 1);
    s->l = l;
    s->hash = s_hash(str);
    s->str = str;
    return s;
}

str_t *s_concat(str_t *l, str_t *r) {
    char *new = strncat(l->str, r->str, l->l + r->l);
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
