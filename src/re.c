#include "types.h"

#include "table.h"

#include <inttypes.h>
#include <stdio.h>

static rf_tbl *fldv;
static pcre2_compile_context *context = NULL;

// Register the VM's global fields table
void re_register_fldv(rf_tbl *t) {
    fldv = t;
    return;
}

rf_re *re_compile(char *pattern, uint32_t flags, int *errcode) {
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

int re_store_numbered_captures(pcre2_match_data *md) {
    uint32_t i = 0;
    while (1) {
        PCRE2_UCHAR *buf;
        PCRE2_SIZE   l;
        if (!pcre2_substring_get_bynumber(md, i, &buf, &l)) {
            rf_val v = (rf_val) {TYPE_STR, .u.s = s_newstr((const char *) buf, l, 0)};
            t_insert_int(fldv, (rf_int) i, &v, 1, 0);
        } else {
            break;
        }
        ++i;
    }
    return 0;
}

rf_int re_match(char *s, rf_re *re, int capture) {

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
    if (capture)
        re_store_numbered_captures(md);

    // Free the PCRE2 match data
    pcre2_match_data_free(md);
    return (rf_int) (rc > 0);
}
