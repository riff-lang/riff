#include <inttypes.h>
#include <stdio.h>

#include "types.h"

#define CFLAGS REG_EXTENDED

rf_re *re_compile(char *pattern) {
    rf_re *regex = malloc(sizeof(rf_re));
    regcomp(regex, pattern, CFLAGS);
    return regex;
}

void re_free(rf_re *regex) {
    regfree(regex);
    return;
}

rf_int re_match(char *s, rf_re *regex) {
    return !regexec(regex, s, 0, NULL, 0);
}
