#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "util.h"

rf_str *s_newstr(const char *start, size_t l, int h) {
    char *str = malloc(l * sizeof(char) + 1);
    memcpy(str, start, l);
    str[l] = '\0';
    rf_str *s = malloc(sizeof(rf_str));
    s->l = l;
    s->hash = h ? u_strhash(str) : 0;
    s->str = str;
    return s;
}

// Assumes null-terminated strings
rf_str *s_newstr_concat(char *l, char *r, int h) {
    size_t l_len = strlen(l);
    size_t r_len = strlen(r);
    size_t new_len = l_len + r_len;
    char *new = malloc(new_len * sizeof(char) + 1);
    memcpy(new, l, l_len);
    memcpy(new + l_len, r, r_len);
    new[new_len] = '\0';
    rf_str *s = malloc(sizeof(rf_str));
    s->l = new_len;
    s->hash = h ? u_strhash(new) : 0;
    s->str = new;
    return s;
}

rf_str *s_substr(char *s, rf_int from, rf_int to, rf_int itvl) {
    // Correct out-of-bounds ranges
    size_t sl = strlen(s);
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

    len = (size_t) ceil(fabs(len / (double) itvl));
    char *str = malloc(len * sizeof(char) + 1);
    for (size_t i = 0; i <= len; ++i) {
        str[i] = s[from];
        from += itvl;
    }
    str[len] = '\0';
    rf_str *ns = malloc(sizeof(rf_str));
    ns->str = str;
    ns->l = len;
    ns->hash = 0;
    return ns;
}

rf_str *s_int2str(rf_int i) {
    char str[32];
    size_t len = sprintf(str, "%"PRId64, i);
    return s_newstr(str, len, 0);
}

rf_str *s_flt2str(rf_flt f) {
    char str[32];
    size_t len = sprintf(str, "%g", f);
    return s_newstr(str, len, 0);
}
