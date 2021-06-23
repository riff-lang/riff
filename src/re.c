#include <inttypes.h>
#include <stdio.h>

#include "table.h"
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

rf_int re_match(char *s, rf_re *re, rf_tbl *fldv) {

    // Create PCRE2 match data block
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);

    // Perform match
    int rc = pcre2_match(
            re,                     // Compiled regex
            (PCRE2_SPTR) s,         // String to match against
            PCRE2_ZERO_TERMINATED,  // Length (or specify zero terminated)
            0,                      // Start offset
            0,                      // Options/flags
            md,                     // Match data block
            NULL);                  // Match context

    // Insert captured substrings into the VM's field vector
    for (uint32_t i = 0; i < rc; ++i) {
        PCRE2_UCHAR *buf;
        PCRE2_SIZE   n;
        pcre2_substring_get_bynumber(md, i, &buf, &n);
        rf_val v = (rf_val) {TYPE_STR, .u.s = s_newstr((const char *) buf, n, 0)};
        t_insert_int(fldv, (rf_int) i, &v, 1, 0);
    }

    // Free the PCRE2 match data
    pcre2_match_data_free(md);
    return (rf_int) (rc > 0);
}
