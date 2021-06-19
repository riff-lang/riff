#include <inttypes.h>
#include <stdio.h>

#include "types.h"

rf_re *re_compile(char *pattern, int flags, int *errcode) {
    PCRE2_SIZE erroffset;

    return pcre2_compile(
            (PCRE2_SPTR) pattern,   // Raw pattern string
            PCRE2_ZERO_TERMINATED,  // Length (or specify zero terminated)
            flags | RE_CFLAGS,      // Options/flags
            errcode,                // Error code
            &erroffset,             // Error offset
            NULL);                  // Compile context
}

void re_free(rf_re *re) {
    pcre2_code_free(re);
    return;
}

rf_int re_match(char *s, rf_re *re) {
    pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(re, NULL);
    int rc = pcre2_match(
            re,                     // Compiled regex
            (PCRE2_SPTR) s,         // String to match against
            PCRE2_ZERO_TERMINATED,  // Length (or specify zero terminated)
            0,                      // Start offset
            0,                      // Options/flags
            match_data,             // Match data block
            NULL);                  // Match context
    pcre2_match_data_free(match_data);
    return (rf_int) (rc > 0);
}

int re_sub(char *s, rf_re *re, char *repl, char *new, size_t *n, int flags) {
    return pcre2_substitute(
            re,                     // Compiled regex
            (PCRE2_SPTR) s,         // Original string pointer
            PCRE2_ZERO_TERMINATED,  // Original string length
            0,                      // Start offset
            flags,                  // Options/flags
            NULL,                   // Match data block
            NULL,                   // Match context
            (PCRE2_SPTR) repl,      // Replacement string pointer
            PCRE2_ZERO_TERMINATED,  // Replacement string length
            (PCRE2_UCHAR *) new,    // Buffer for new string
            n);                     // Buffer size (overwritten w/ length)
}
