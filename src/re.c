#include "types.h"

#define CFLAGS REG_EXTENDED

rf_re *re_compile(char *pattern) {
    rf_re *regex = malloc(sizeof(rf_re));
    regcomp(regex, pattern, CFLAGS);
    return regex;
}

rf_int re_match(rf_str *s, rf_re *regex) {
    return !regexec(regex, s->str, 0, NULL, 0);
}
