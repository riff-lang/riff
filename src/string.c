#include "types.h"

#include "util.h"

#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Create string and skip hashing
rf_str *s_new(const char *start, size_t l) {
    char *str = malloc(l * sizeof(char) + 1);
    memcpy(str, start, l);
    str[l] = '\0';
    rf_str *s = malloc(sizeof(rf_str));
    *s = (rf_str) {0, l, str};
    return s;
}

// Create string with hash
rf_str *s_newh(const char *start, size_t l) {
    char *str = malloc(l * sizeof(char) + 1);
    memcpy(str, start, l);
    str[l] = '\0';
    rf_str *s = malloc(sizeof(rf_str));
    *s = (rf_str) {u_strhash(str), l, str};
    return s;
}

// Assumes null-terminated strings
rf_str *s_new_concat(char *l, char *r) {
    size_t l_len = strlen(l);
    size_t r_len = strlen(r);
    size_t new_len = l_len + r_len;
    char *new = malloc(new_len * sizeof(char) + 1);
    memcpy(new, l, l_len);
    memcpy(new + l_len, r, r_len);
    new[new_len] = '\0';
    rf_str *s = malloc(sizeof(rf_str));
    *s = (rf_str) {0, new_len, new};
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
    for (size_t i = 0; i < len; ++i) {
        str[i] = s[from];
        from += itvl;
    }
    str[len] = '\0';
    rf_str *ns = malloc(sizeof(rf_str));
    *ns = (rf_str) {0, len, str};
    return ns;
}
