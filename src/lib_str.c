#include "lib.h"

#include "fmt.h"
#include "string.h"

#include <ctype.h>
#include <errno.h>
#include <string.h>

// String functions

// byte(s[,i])
// Takes one string and an optional index argument `i` and returns the
// numeric ASCII code associated with that character. The default
// value for `i` is 0.
LIB_FN(byte) {
    int idx = argc > 1 ? intval(fp+1) : 0;
    if (is_str(fp)) {
        if (idx > riff_strlen(fp->s)) {
            idx = riff_strlen(fp->s);
        }
        set_int(fp-1, (uint8_t) fp->s->str[idx]);
    } else {
        set_int(fp-1, 0);
    }
    return 1;
}

// char(...)
// Takes zero or more integers and returns a string composed of the
// character codes of each respective argument in order
// Ex:
//   char(114, 105, 102, 102) -> "riff"
LIB_FN(char) {
    if (!argc) {
        return 0;
    }
    char buf[STR_BUF_SZ];
    int n = build_char_str(fp, argc, buf);
    set_str(fp-1, riff_str_new(buf, n));
    return 1;
}

// fmt(...)
LIB_FN(fmt) {
    if (!is_str(fp)) {
        return 0;
    }
    --argc;
    char buf[STR_BUF_SZ];
    int n = fmt_snprintf(buf, sizeof buf, fp->s->str, fp + 1, argc);
    set_str(fp-1, riff_str_new(buf, n));
    return 1;
}

static int xsub(riff_val *fp, int argc, int flags) {
    char  *s;
    riff_regex *p;
    char  *r;
    size_t len = 0;

    char temp_s[32];
    char temp_r[32];

    // String `s`
    if (!is_str(fp)) {
        if (is_int(fp))
            len = riff_lltostr(fp->i, temp_s);
        else if (is_float(fp))
            len = riff_dtostr(fp->f, temp_s);
        else
            return 0;
        s = temp_s;
    } else {
        s = fp->s->str;
        len = riff_strlen(fp->s);
    }

    // Pattern `p`
    if (!is_regex(fp+1)) {
        int errcode;
        if (is_num(fp+1)) {
            char temp_p[32];
            if (is_int(fp+1))
                riff_lltostr(fp[1].i, temp_p);
            else if (is_float(fp+1))
                riff_dtostr(fp[1].f, temp_p);
            p = re_compile(temp_p, PCRE2_ZERO_TERMINATED, 0, &errcode);
        } else if (is_str(fp+1)) {
            p = re_compile(fp[1].s->str, riff_strlen(fp[1].s), 0, &errcode);
        } else {
            return 0;
        }
    } else {
        p = fp[1].r;
    }

    // If replacement `r` provided
    if (argc > 2) {
        if (!is_str(fp+2)) {
            if (is_int(fp+2))
                riff_lltostr(fp[2].i, temp_r);
            else if (is_float(fp+2))
                riff_dtostr(fp[2].f, temp_r);
            else
                temp_r[0] = '\0';
            r = temp_r;
        } else {
            r = fp[2].s->str;
        }
    }

    // Otherwise, treat `r` as an empty string, effectively deleting
    // substrings matching `p` from `s`.
    else {
        temp_r[0] = '\0';
        r = temp_r;
    }

    char   buf[STR_BUF_SZ];
    size_t n = STR_BUF_SZ;

    // Create match data for storing captured subexpressions
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(p, NULL);

    // In order to properly capture substrings resulting from the
    // substitution pattern, PCRE2 match data must be passed to a
    // PCRE2 match operation with the same pattern and subject string
    // before performing the actual subtitution
    pcre2_match(p, (PCRE2_SPTR) s, len, 0, 0, md, NULL);

    // Perform the substitution
    pcre2_substitute(
            p,                      // Compiled regex
            (PCRE2_SPTR) s,         // Original string pointer
            len,                    // Original string length
            0,                      // Start offset
            PCRE2_SUBSTITUTE_MATCHED
            | flags,                // Options/flags
            md,                     // Match data block
            NULL,                   // Match context
            (PCRE2_SPTR) r,         // Replacement string pointer
            PCRE2_ZERO_TERMINATED,  // Replacement string length
            (PCRE2_UCHAR *) buf,    // Buffer for new string
            &n);                    // Buffer size (overwritten w/ length)

    // Store capture substrings in the global fields table
    re_store_numbered_captures(md);
    pcre2_match_data_free(md);
    set_str(fp-1, riff_str_new(buf, n));
    return 1;
}

// [g]sub(s,p[,r])
// Returns a copy of string `s` where all occurrences (gsub) or the
// first occurrence (sub) of pattern `p` are replaced by string `r`
LIB_FN(gsub) { return xsub(fp, argc, PCRE2_SUBSTITUTE_GLOBAL); }
LIB_FN(sub)  { return xsub(fp, argc, 0);                       }

// hex(x)
// Returns a string of `x` as an integer in hexadecimal (lowercase)
// with the leading "0x"
// Ex:
//   hex(255) -> "0xff"
LIB_FN(hex) {
    riff_int i = intval(fp);
    char buf[20];
    size_t len = sprintf(buf, "0x%"PRIx64, i);
    set_str(fp-1, riff_str_new(buf, len));
    return 1;
}

static int allxcase(riff_val *fp, int c) {
    if (!is_str(fp))
        return 0;
    size_t len = riff_strlen(fp->s);
    char str[len + 1];
    for (int i = 0; i < len; ++i) {
        str[i] = c ? toupper(fp->s->str[i])
                   : tolower(fp->s->str[i]);
    }
    str[len] = '\0';
    set_str(fp-1, riff_str_new(str, len));
    return 1;
}

// lower/upper(s)
LIB_FN(lower) { return allxcase(fp, 0); }
LIB_FN(upper) { return allxcase(fp, 1); }

// split(s[,d])
// Returns a table with elements being string `s` split on delimiter
// `d`, treated as a regular expression. If no delimiter is provided,
// the regular expression /\s+/ (whitespace) is used. If the delimiter
// is the empty string (""), the string is split into a table of
// single-byte strings.
LIB_FN(split) {
    char *str;
    size_t len = 0;
    char temp_s[20];
    if (!is_str(fp)) {
        if (is_int(fp)) {
            len = riff_lltostr(fp->i, temp_s);
        } else if (is_float(fp)) {
            len = riff_dtostr(fp->f, temp_s);
        } else {
            return 0;
        }
        str = temp_s;
    } else {
        str = fp->s->str;
        len = riff_strlen(fp->s);
    }
    riff_str *s;
    riff_tab *t = malloc(sizeof(riff_tab));
    riff_tab_init(t);
    riff_regex *delim;
    int errcode = 0;
    if (argc < 2) {
        delim = re_compile("\\s+", PCRE2_ZERO_TERMINATED, 0, &errcode);
    } else if (!is_regex(fp+1)) {
        char temp[32];
        switch (fp[1].type) {
        case TYPE_INT: riff_lltostr(fp[1].i, temp); break;
        case TYPE_FLOAT: riff_dtostr(fp[1].f, temp); break;
        case TYPE_STR:
            if (!riff_strlen(fp[1].s))
                goto split_chars;
            delim = re_compile(fp[1].s->str, riff_strlen(fp[1].s), 0, &errcode);
            goto do_split;
        default:
            goto split_chars;
        }
        delim = re_compile(temp, PCRE2_ZERO_TERMINATED, 0, &errcode);
    } else {
        delim = fp[1].r;
    }

    // Split on regular expression
do_split: {
    char buf[STR_BUF_SZ];
    char *p = buf;
    size_t n = STR_BUF_SZ;
    char *sentinel = "\0";
    pcre2_substitute(
            delim,
            (PCRE2_SPTR) str,
            len,
            0,
            PCRE2_SUBSTITUTE_GLOBAL,
            NULL,
            NULL,
            (PCRE2_SPTR) sentinel,
            1,
            (PCRE2_UCHAR *) buf,
            &n);
    n += 1;
    // Extra null terminator, since the '\0' at buf[n] is a sentinel
    // value
    buf[n] = '\0';
    for (riff_int i = 0; n > 0;) {
        if (!*p) {
            ++p;
            --n;
        } else {
            size_t l = strlen(p);
            s = riff_str_new(p, l);
            p += l + 1;
            n -= l + 1;
            riff_tab_insert_int(t, i++, &(riff_val) {TYPE_STR, .s = s});
        }
    }
    set_tab(fp-1, t);
    return 1;
    }

    // Split into single-byte strings
split_chars: {
    for (riff_int i = 0; i < len; ++i) {
        s = riff_str_new(str + i, 1);
        riff_tab_insert_int(t, i, &(riff_val) {TYPE_STR, .s = s});
    }
    set_tab(fp-1, t);
    return 1;
    }
}

static riff_lib_fn_reg strlib[] = {
    LIB_FN_REG(byte,  1),
    LIB_FN_REG(char,  0),
    LIB_FN_REG(fmt,   1),
    LIB_FN_REG(gsub,  2),
    LIB_FN_REG(hex,   1),
    LIB_FN_REG(lower, 1),
    LIB_FN_REG(split, 1),
    LIB_FN_REG(sub,   2),
    LIB_FN_REG(upper, 1),
};

void riff_lib_register_str(riff_htab *g) {
    FOREACH(strlib, i) {
        riff_htab_insert_cstr(g, strlib[i].name, &(riff_val) {TYPE_CFN, .cfn = &strlib[i].fn});
    }
}
