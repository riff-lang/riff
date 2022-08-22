#include "value.h"

#include "conf.h"
#include "string.h"
#include "table.h"

#include <inttypes.h>
#include <stdio.h>

static riff_tab *fldv;
static pcre2_compile_context *context = NULL;

// Register the VM's global fields table
void re_register_fldv(riff_tab *t) {
    fldv = t;
    return;
}

riff_regex *re_compile(char *pattern, size_t len, uint32_t flags, int *errcode) {
    if (context == NULL) {
        context = pcre2_compile_context_create(NULL);
        pcre2_set_compile_extra_options(context, RE_CFLAGS_EXTRA);
    }

    PCRE2_SIZE erroffset;
    riff_regex *r = pcre2_compile(
            (PCRE2_SPTR) pattern,   // Raw pattern string
            len,                    // Length (or specify zero terminated)
            flags | RE_CFLAGS,      // Options/flags
            errcode,                // Error code
            &erroffset,             // Error offset
            context);               // Compile context
    return r;
}

void re_free(riff_regex *re) {
    pcre2_code_free(re);
    return;
}

int re_store_numbered_captures(pcre2_match_data *md) {
    uint32_t i = 0;
    PCRE2_UCHAR buf[STR_BUF_SZ];
    while (1) {
        PCRE2_SIZE l = STR_BUF_SZ;
        if (!pcre2_substring_copy_bynumber(md, i, buf, &l)) {
            riff_val v = (riff_val) {TYPE_STR, .s = riff_str_new((const char *) buf, l)};
            riff_tab_insert_int(fldv, (riff_int) i, &v);
        } else {
            break;
        }
        ++i;
    }
    return 0;
}

riff_int re_match(char *s, size_t len, riff_regex *re, int capture) {

    // Create PCRE2 match data block
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);

    // Perform match
    int rc = pcre2_match(
            re,                     // Compiled regex
            (PCRE2_SPTR) s,         // String to match against
            len,                    // Length (or specify zero terminated)
            0,                      // Start offset
            0,                      // Options/flags
            md,                     // Match data block
            NULL);                  // Match context

    // Insert captured substrings into the VM's field vector
    if (capture)
        re_store_numbered_captures(md);

    // Free the PCRE2 match data
    pcre2_match_data_free(md);
    return (riff_int) (rc > 0);
}
