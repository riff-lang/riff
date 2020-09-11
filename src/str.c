#include <math.h>
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

rf_str *s_newstr(const char *start, size_t l, int h) {
    char *str = malloc(l * sizeof(char) + 1);
    memcpy(str, start, l);
    str[l] = '\0';
    rf_str *s = malloc(sizeof(rf_str));
    s->l = l;
    s->hash = h ? s_hash(str) : 0;
    s->str = str;
    return s;
}

rf_str *s_substr(rf_str *s, rf_int from, rf_int to, rf_int itvl) {
    // Correct out-of-bounds ranges
    rf_int sl = (rf_int) s->l;
    from = from > sl ? sl : from < 0 ? 0 : from;
    to   = to   > sl ? sl : to   < 0 ? 0 : to;

    // Disallow contradicting directions. E.g. negative intervals when
    // from > to. Infer the direction from the from/to range and
    // override the provided interval with its negative value.
    if ((from < to && itvl < 1) ||
        (from > to && itvl > -1))
        itvl = -itvl;

    size_t len = (size_t) llabs(to - from) + 1;
    len = (size_t) ceil(fabs(len / (double) itvl));
    char *str = malloc(len * sizeof(char) + 1);
    for (size_t i = 0; i <= len; ++i) {
        str[i] = s->str[from];
        from += itvl;
    }
    str[len] = '\0';
    rf_str *ns = malloc(sizeof(rf_str));
    ns->str = str;
    ns->l = len;
    ns->hash = 0;
    return ns;
}

rf_str *s_concat(rf_str *l, rf_str *r, int h) {
    size_t nl = l->l + r->l;
    char *new = malloc(nl * sizeof(char) + 1);
    memcpy(new, l->str, l->l);
    memcpy(new + l->l, r->str, r->l);
    new[nl] = '\0';
    rf_str *s = malloc(sizeof(rf_str));
    s->l = nl;
    s->hash = h ? s_hash(new) : 0;
    s->str = new;
    return s;
}

rf_str *s_int2str(rf_int i) {
    size_t len = snprintf(NULL, 0, "%lld", i);
    char buf[len + 1];
    snprintf(buf, len + 1, "%lld", i);
    return s_newstr(buf, len, 0);
}

rf_str *s_flt2str(rf_flt f) {
    size_t len = snprintf(NULL, 0, "%g", f);
    char buf[len + 1];
    snprintf(buf, len + 1, "%g", f);
    return s_newstr(buf, len, 0);
}
