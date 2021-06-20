#include <inttypes.h>
#include <stdio.h>

#include "types.h"

static pcre2_compile_context *context = NULL;

rf_re *re_compile(char *pattern, int flags, int *errcode) {
    if (context == NULL) {
        context = pcre2_compile_context_create(NULL);
        pcre2_set_compile_extra_options(context, RE_CFLAGS_EXTRA);
    }

    PCRE2_SIZE erroffset;
    rf_re *r = pcre2_compile(
            (PCRE2_SPTR) pattern,   // Raw pattern string
            PCRE2_ZERO_TERMINATED,  // Length (or specify zero terminated)
            flags | RE_CFLAGS,      // Options/flags
            errcode,                // Error code
            &erroffset,             // Error offset
            context);               // Compile context
    return r;
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
