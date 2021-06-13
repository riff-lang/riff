#include <inttypes.h>
#include <stdio.h>

#include "types.h"

#define CFLAGS REG_EXTENDED // Extended Regex by default

rf_re *re_compile(char *pattern, int flags) {
    rf_re *regex = malloc(sizeof(rf_re));
    regcomp(regex, pattern, flags | CFLAGS);
    return regex;
}

void re_free(rf_re *regex) {
    regfree(regex);
    return;
}

rf_int re_match(char *s, rf_re *regex) {
    return !regexec(regex, s, 0, NULL, 0);
}
