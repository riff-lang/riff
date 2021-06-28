#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "conf.h"
#include "fn.h"
#include "lib.h"
#include "mem.h"
#include "table.h"

static void err(const char *msg) {
    fprintf(stderr, "riff: %s\n", msg);
    exit(1);
}

// Arithmetic functions

// abs(x)
static int l_abs(rf_val *fp, int argc) {
    if (is_int(fp))
        assign_int(fp-1, llabs(fp->u.i));
    else
        assign_flt(fp-1, fabs(fltval(fp)));
    return 1;
}

// atan(y[,x])
static int l_atan(rf_val *fp, int argc) {
    if (argc == 1)
        assign_flt(fp-1, atan(fltval(fp)));
    else if (argc > 1)
        assign_flt(fp-1, atan2(fltval(fp), fltval(fp+1)));
    return 1;
}

// ceil(x)
static int l_ceil(rf_val *fp, int argc) {
    assign_int(fp-1, (rf_int) ceil(fltval(fp)));
    return 1;
}

// cos(x)
static int l_cos(rf_val *fp, int argc) {
    assign_flt(fp-1, cos(fltval(fp)));
    return 1;
}

// exp(x)
static int l_exp(rf_val *fp, int argc) {
    assign_flt(fp-1, exp(fltval(fp)));
    return 1;
}

// int(x)
static int l_int(rf_val *fp, int argc) {
    assign_int(fp-1, intval(fp));
    return 1;
}

// log(x[,b])
static int l_log(rf_val *fp, int argc) {
    if (argc == 1)
        assign_flt(fp-1, log(fltval(fp)));
    else if (fltval(fp+1) == 2.0)
        assign_flt(fp-1, log2(fltval(fp)));
    else if (fltval(fp+1) == 10.0)
        assign_flt(fp-1, log10(fltval(fp)));
    else
        assign_flt(fp-1, log(fltval(fp)) / log(fltval(fp+1)));
    return 1;
}

// sin(x)
static int l_sin(rf_val *fp, int argc) {
    assign_flt(fp-1, sin(fltval(fp)));
    return 1;
}

// sqrt(x)
static int l_sqrt(rf_val *fp, int argc) {
    assign_flt(fp-1, sqrt(fltval(fp)));
    return 1;
}

// tan(x)
static int l_tan(rf_val *fp, int argc) {
    assign_flt(fp-1, tan(fltval(fp)));
    return 1;
}

// Pseudo-random number generation

// rand([x])
// Uses POSIX drand48() to produce random floats and two calls to
// lrand48()/mrand48() concatenated together to produce random 64-bit
// integers.
// Ex:
//   rand()  -> float in the range [0..1)
//   rand(0) -> random Riff integer (64 bit signed)
//   rand(n) -> random integer in the range [0..n]; n can be negative
// TODO handle rand(x,y), returning a random integer in the range of
// [x,y]
static int l_rand(rf_val *fp, int argc) {
    if (!argc) {
        assign_flt(fp-1, drand48());
    } else { 
        if (intval(fp) == 0) {
            int64_t n1 = mrand48();
            int64_t n2 = mrand48();
            rf_int n = (n1 << 32) ^ n2;
            assign_int(fp-1, n);
        } else {
            uint64_t n1 = lrand48();
            uint64_t n2 = lrand48();
            rf_int n = (n1 << 32) ^ n2;
            if (intval(fp) > 0)
                assign_int(fp-1, n % (intval(fp) + 1));
            else
                assign_int(fp-1, -(n % -(intval(fp) - 1)));
        }
    }
    return 1;
}

// srand([x])
// Initializes the PRNG with seed `x` or time(0) if no argument given.
// rand() will produce the same sequence when srand is initialized
// with a given seed every time.
// Ex:
//   srand(99); rand(); rand() -> 0.380257, 0.504358
static int l_srand(rf_val *fp, int argc) {
    if (!argc)
        srand48(time(0));
    else
        srand48((long) intval(fp));
    return 0;
}

// String functions

// byte(s[,i])
// Takes one string and an optional index argument `i` and returns the
// numeric ASCII code associated with that character. The default
// value for `i` is 0.
// If a user-defined function is passed, the byte at index `i` in the
// function's bytecode array is returned. This is the same as
// subscripting the function, i.e. byte(f,0) == f[0] for function f.
static int l_byte(rf_val *fp, int argc) {
    int idx = argc > 1 ? intval(fp+1) : 0;
    if (is_str(fp)) {
        if (idx > fp->u.s->l)
            idx = fp->u.s->l;
        assign_int(fp-1, (uint8_t) fp->u.s->str[idx]);
    } else if (is_rfn(fp)) {
        if (idx > fp->u.fn->code->n)
            idx = fp->u.fn->code->n;
        assign_int(fp-1, fp->u.fn->code->code[idx]);
    } else {
        return 0;
    }
    return 1;
}

// char(...)
// Takes zero or more integers and returns a string composed of the
// character codes of each respective argument in order
// Ex:
//   char(114, 105, 102, 102) -> "riff"
static int l_char(rf_val *fp, int argc) {
    if (!argc) return 0;
    char buf[STR_BUF_SZ];
    int n = 0;
    for (int i = 0; i < argc; ++i) {
        uint32_t c = (uint32_t) intval(fp+i);
        if (c <= 0x7f)
            buf[n++] = (char) c;
        else {
            char ubuf[8];
            int j = 8 - u_unicode2utf8(ubuf, c);
            for (; j < 8; ++j) {
                buf[n++] = ubuf[j];
            }
        }
    }
    assign_str(fp-1, s_newstr(buf, n, 0));
    return 1;
}

// %c
#define fmt_char(b, n, c, left, width) \
    if (left) { \
        n += sprintf(b + n, "%-*c", width, c); \
    } else { \
        n += sprintf(b + n, "%*c", width, c); \
    }

// %s
#define fmt_str(b, n, s, left, width, prec) \
    if (left) { \
        n += sprintf(b + n, "%-*.*s", width, prec, s); \
    } else { \
        n += sprintf(b + n, "%*.*s", width, prec, s); \
    }

// Signed fmt conversions: floats, decimal integers
#define fmt_signed(b, n, i, fmt, left, sign, space, zero, width, prec) \
    if (left) { \
        if (sign) { \
            n += sprintf(b + n, "%-+*.*"fmt, width, prec, i); \
        } else if (space) { \
            n += sprintf(b + n, "%- *.*"fmt, width, prec, i); \
        } else { \
            n += sprintf(b + n, "%-*.*"fmt, width, prec, i); \
        } \
    } else if (zero) { \
        if (sign) { \
            n += sprintf(b + n, "%+0*.*"fmt, width, prec, i); \
        } else if (space) { \
            n += sprintf(b + n, "% 0*.*"fmt, width, prec, i); \
        } else { \
            n += sprintf(b + n, "%0*.*"fmt, width, prec, i); \
        } \
    } else if (sign) { \
        n += sprintf(b + n, "%+*.*"fmt, width, prec, i); \
    } else if (space) { \
        n += sprintf(b + n, "% *.*"fmt, width, prec, i); \
    } else { \
        n += sprintf(b + n, "%*.*"fmt, width, prec, i); \
    }

// Unsigned fmt conversions: hex and octal integers
// Only difference is absence of space flag, since numbers are
// converted to unsigned anyway. clang also throws a warning
// about UB for octal/hex conversions with the space flag.
#define fmt_unsigned(b, n, i, fmt, left, zero, width, prec) \
    if (left) { \
        n += sprintf(b + n, "%-*.*"fmt, width, prec, i); \
    } else if (zero) { \
        n += sprintf(b + n, "%0*.*"fmt, width, prec, i); \
    } else { \
        n += sprintf(b + n, "%*.*"fmt, width, prec, i); \
    }

// fmt(...)
// Riff's `sprintf()` implementation. Doubles as `printf()` due to the
// implicit printing functionality of the language.
//
// Optional modifiers (zero or more):
//   -              | Left-justified
//   <space>        | Space-padded (ignored if left-justified)
//   0              | Zero-padded (ignored if left-justified)
//   <integer> / *  | Field width
//   .              | Precision specifier
//
// Format specifiers:
//   %              | Literal `%`
//   a / A          | Hex float (exp notation; lowercase/uppercase)
//   c              | Single character
//   d / i          | Decimal integer
//   e / E          | Decimal float (exp notation)
//   f / F          | Decimal float
//   g              | `e` or `f` conversion (whichever is shorter)
//   G              | `E` or `F` conversion (whichever is shorter)
//   o              | Octal integer
//   s              | String
//   x / X          | Hex integer (lowercase/uppercase)
static int l_fmt(rf_val *fp, int argc) {
    if (!is_str(fp))
        return 0;
    --argc;
    int arg = 1;

    const char *fstr = fp->u.s->str;

    char buf[STR_BUF_SZ];
    int  n = 0;

    // Flags and specifiers
    int left, sign, space, zero, width, prec;

    while (*fstr && argc && n <= STR_BUF_SZ) {
        if (*fstr != '%') {
            buf[n++] = *fstr++;
            continue;
        }

        // Advance pointer and check for literal '%'
        if (*++fstr == '%') {
            buf[n++] = '%';
            ++fstr;
            continue;
        }

        left  = 0;
        sign  = 0;
        space = 0;
        zero  = 0;

        width = -1;

        // Both clang and gcc seem to allow -1 to be used as a
        // precision modifier without throwing warnings, so this is a
        // useful default
        prec = -1;

flags:  // Capture flags
        switch (*fstr) {
        case '0': zero  = 1; ++fstr; goto flags;
        case '+': sign  = 1; ++fstr; goto flags;
        case ' ': space = 1; ++fstr; goto flags;
        case '-': left  = 1; ++fstr; goto flags;
        default:  break;
        }

        // Evaluate field width
        if (isdigit(*fstr)) {
            char *end;
            width = (int) strtol(fstr, &end, 10);
            fstr = end;
        } else if (*fstr == '*') {
            ++fstr;
            if (argc--) {
                width = (int) intval(fp+arg);
                ++arg;
            }
        }

        // Evaluate precision modifier
        if (*fstr == '.') {
            ++fstr;
            if (isdigit(*fstr)) {
                char *end;
                prec = (int) strtol(fstr, &end, 10);
                fstr = end;
            } else if (*fstr == '*') {
                ++fstr;
                if (argc--) {
                    prec = (int) intval(fp+arg);
                    ++arg;
                }
            } else {
                prec = 0;
            }
        }

        rf_int i;
        rf_flt f;

        // TODO binary/%b?
        // Evaluate format specifier
        switch (*fstr++) {
        case 'c': {
            if (argc--) {
                int c = (int) intval(fp+arg);
                fmt_char(buf, n, c, left, width);
                ++arg;
            }
            break;
        }
        case 'd': case 'i': {
            if (argc--) {
redir_int:
                i = intval(fp+arg);
                fmt_signed(buf, n, i, PRId64, left, sign, space, zero, width, prec);
                ++arg;
            }
            break;
        }
        case 'o':
            if (argc--) {
                i = intval(fp+arg);
                fmt_unsigned(buf, n, i, PRIo64, left, zero, width, prec);
                ++arg;
            }
            break;
        case 'x':
            if (argc--) {
                i = intval(fp+arg);
                fmt_unsigned(buf, n, i, PRIx64, left, zero, width, prec);
                ++arg;
            }
            break;
        case 'X':
            if (argc--) {
                i = intval(fp+arg);
                fmt_unsigned(buf, n, i, PRIX64, left, zero, width, prec);
                ++arg;
            }
            break;
        case 'a':
            if (argc--) {
                f = fltval(fp+arg);
                // Default precision left as -1 for `a`
                fmt_signed(buf, n, f, "a", left, sign, space, zero, width, prec);
                ++arg;
            }
            break;
        case 'A':
            if (argc--) {
                f = fltval(fp+arg);
                // Default precision left as -1 for `A`
                fmt_signed(buf, n, f, "A", left, sign, space, zero, width, prec);
                ++arg;
            }
            break;
        case 'e':
            if (argc--) {
                f = fltval(fp+arg);
                prec = prec < 0 ? DEFAULT_FLT_PREC : prec;
                fmt_signed(buf, n, f, "e", left, sign, space, zero, width, prec);
                ++arg;
            }
            break;
        case 'E':
            if (argc--) {
                f = fltval(fp+arg);
                prec = prec < 0 ? DEFAULT_FLT_PREC : prec;
                fmt_signed(buf, n, f, "E", left, sign, space, zero, width, prec);
                ++arg;
            }
            break;
        case 'f': case 'F':
            if (argc--) {
                f = fltval(fp+arg);
                prec = prec < 0 ? DEFAULT_FLT_PREC : prec;
                fmt_signed(buf, n, f, "f", left, sign, space, zero, width, prec);
                ++arg;
            }
            break;
        case 'g':
            if (argc--) {
redir_flt:
                f = fltval(fp+arg);
                prec = prec < 0 ? DEFAULT_FLT_PREC : prec;
                fmt_signed(buf, n, f, "g", left, sign, space, zero, width, prec);
                ++arg;
            }
            break;
        case 'G':
            if (argc--) {
                f = fltval(fp+arg);
                prec = prec < 0 ? DEFAULT_FLT_PREC : prec;
                fmt_signed(buf, n, f, "G", left, sign, space, zero, width, prec);
                ++arg;
            }
            break;

        // %s should accept any type; redirect as needed
        case 's':
            if (argc--) {
                if (is_str(fp+arg)) {
                    fmt_str(buf, n, fp[arg].u.s->str, left, width, prec);
                } else if (is_int(fp+arg)) {
                    goto redir_int;
                } else if (is_flt(fp+arg)) {
                    goto redir_flt;
                }

                // TODO handle other types
                else {
                    fmt_str(buf, n, "", left, width, prec);
                }
                ++arg;
            }
            break;
        default:
            // Throw error
            err("[fmt] invalid format specifier");
        }

        if (n >= STR_BUF_SZ)
            err("[fmt] string length exceeds maximum buffer size");
    }

    // Copy rest of string after exhausting user-provided args
    while (*fstr && n <= STR_BUF_SZ) {
        buf[n++] = *fstr++;
    }

    assign_str(fp-1, s_newstr(buf, n, 0));
    return 1;
}

static int xsub(rf_val *fp, int argc, int flags) {
    char  *s;
    rf_re *p;
    char  *r;

    char temp_s[32];
    char temp_r[32];

    // String `s`
    if (!is_str(fp)) {
        if (is_int(fp))
            u_int2str(fp->u.i, temp_s, 32);
        else if (is_flt(fp))
            u_flt2str(fp->u.f, temp_s, 32);
        else
            return 0;
        s = temp_s;
    } else {
        s = fp->u.s->str;
    }

    // Pattern `p`
    if (!is_re(fp+1)) {
        int errcode;
        if (is_num(fp+1)) {
            char temp_p[32];
            if (is_int(fp+1))
                u_int2str(fp[1].u.i, temp_p, 32);
            else if (is_flt(fp+1))
                u_flt2str(fp[1].u.f, temp_p, 32);
            p = re_compile(temp_p, 0, &errcode);
        } else if (is_str(fp+1)) {
            p = re_compile(fp[1].u.s->str, 0, &errcode);
        } else {
            return 0;
        }
    } else {
        p = fp[1].u.r;
    }

    // If replacement `r` provided
    if (argc > 2) {
        if (!is_str(fp+2)) {
            if (is_int(fp+2))
                u_int2str(fp[2].u.i, temp_r, 32);
            else if (is_flt(fp+2))
                u_flt2str(fp[2].u.f, temp_r, 32);
            else
                temp_r[0] = '\0';
            r = temp_r;
        } else {
            r = fp[2].u.s->str;
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
    int m_res = pcre2_match(
            p,
            (PCRE2_SPTR) s,
            PCRE2_ZERO_TERMINATED,
            0,
            0,
            md,
            NULL);

    // Perform the substitution
    int res = pcre2_substitute(
            p,                      // Compiled regex
            (PCRE2_SPTR) s,         // Original string pointer
            PCRE2_ZERO_TERMINATED,  // Original string length
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
    assign_str(fp-1, s_newstr(buf, n, 0));
    return 1;
}

// gsub(s,p[,r])
// Returns a copy of string `s` where all occurrences of pattern `p`
// are replaced by string `r`
static int l_gsub(rf_val *fp, int argc) {
    return xsub(fp, argc, PCRE2_SUBSTITUTE_GLOBAL);
}

// hex(x)
// Returns a string of `x` as an integer in hexadecimal (lowercase)
// with the leading "0x"
// Ex:
//   hex(255) -> "0xff"
static int l_hex(rf_val *fp, int argc) {
    rf_int i = intval(fp);
    char buf[20];
    int len = sprintf(buf, "0x%"PRIx64, i);
    assign_str(fp-1, s_newstr(buf, len, 0));
    return 1;
}

static int allxcase(rf_val *fp, int c) {
    size_t len = fp->u.s->l;
    char str[len + 1];
    for (int i = 0; i < len; ++i) {
        str[i] = c ? toupper(fp->u.s->str[i])
                   : tolower(fp->u.s->str[i]);
    }
    str[len] = '\0';
    assign_str(fp-1, s_newstr(str, len, 0));
    return 1;
}

// lower(s)
static int l_lower(rf_val *fp, int argc) {
    if (!is_str(fp))
        return 0;
    return allxcase(fp, 0);
}

// num(s[,b])
// Takes a string `s` and an optional base `b` and returns the
// interpreted num. The base can be 0 or an integer in the range
// {2..36}. The default base is 0. This function is more or less a
// direct interface to `strtoll()`.
static int l_num(rf_val *fp, int argc) {
    if (!is_str(fp)) {
        if (is_int(fp)) {
            assign_int(fp-1, fp->u.i);
        } else if (is_flt(fp)) {
            assign_flt(fp-1, fp->u.f);
        } else {
            assign_int(fp-1, 0);
        }
        return 1;
    }
    int base = 0;
    if (argc > 1)
        base = intval(fp+1);
    char *end;
    errno = 0;
    rf_int i = u_str2i64(fp->u.s->str, &end, base);
    if (errno == ERANGE || isdigit(*end))
        goto ret_flt;
    if (*end == '.') {
        if ((base == 10 && isdigit(end[1])) ||
            (base == 16 && isxdigit(end[1])))
            goto ret_flt;
    } else if (base == 10 && (*end == 'e' || *end == 'E')) {
        goto ret_flt;
    } else if (base == 16 && (*end == 'p' || *end == 'P')) {
        goto ret_flt;
    }
    assign_int(fp-1, i);
    return 1;
ret_flt:
    assign_flt(fp-1, u_str2d(fp->u.s->str, &end, base));
    return 1;
}

// split(s[,d])
// Returns a table with elements being string `s` split on delimiter
// `d`, treated as a regular expression. If no delimiter is provided,
// the regular expression /\s+/ (whitespace) is used. If the delimiter
// is the empty string (""), the string is split into a table of
// single-byte strings.
static int l_split(rf_val *fp, int argc) {
    char *str;
    size_t len;
    char temp_s[20];
    if (!is_str(fp)) {
        if (is_int(fp))
            len = sprintf(temp_s, "%"PRId64, fp->u.i);
        else if (is_flt(fp))
            len = sprintf(temp_s, "%g", fp->u.f);
        else
            return 0;
        temp_s[len] = '\0';
        str = temp_s;
    } else {
        str = fp->u.s->str;
        len = fp->u.s->l;
    }
    rf_str *s;
    rf_val v;
    rf_val *tbl = v_newtbl();
    rf_re *delim;
    int errcode = 0;
    if (argc < 2) {
        delim = re_compile("\\s+", 0, &errcode);
    } else if (!is_re(fp+1)) {
        char temp[32];
        switch (fp[1].type) {
        case TYPE_INT: u_int2str(fp[1].u.i, temp, 32); break;
        case TYPE_FLT: u_flt2str(fp[1].u.f, temp, 32); break;
        case TYPE_STR:
            if (!fp[1].u.s->l)
                goto split_chars;
            delim = re_compile(fp[1].u.s->str, 0, &errcode);
            goto do_split;
        default:
            goto split_chars;
        }
        delim = re_compile(temp, 0, &errcode);
    } else {
        delim = fp[1].u.r;
    }

    // Split on regular expression
do_split: {
    char buf[STR_BUF_SZ];
    char *p = buf;
    size_t n = STR_BUF_SZ;
    char *sentinel = "\0";
    int rc = pcre2_substitute(
            delim,
            (PCRE2_SPTR) str,
            PCRE2_ZERO_TERMINATED,
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
    for (rf_int i = 0; n > 0; ++i) {
        // Insert an empty string when the delimiter produces
        // consecutive sentinel values
        if (!*p) {
            s = s_newstr("", 0, 0);
            ++p;
            --n;
        } else {
            size_t l = strlen(p);
            s = s_newstr(p, l, 0);
            p += l + 1;
            n -= l + 1;
        }
        v = (rf_val) {TYPE_STR, .u.s = s};
        t_insert_int(tbl->u.t, i, &v, 1, 1);
    }
    fp[-1] = *tbl;
    return 1;
    }

    // Split into single-byte strings
split_chars: {
    for (rf_int i = 0; i < len; ++i) {
        s = s_newstr(str + i, 1, 0);
        v = (rf_val) {TYPE_STR, .u.s = s};
        t_insert_int(tbl->u.t, i, &v, 1, 1);
    }
    fp[-1] = *tbl;
    return 1;
    }
}

// sub(s,p[,r])
// Returns a copy of string `s` where only the first occurrence of
// pattern `p` is replaced by string `r`
static int l_sub(rf_val *fp, int argc) {
    return xsub(fp, argc, 0);
}

// type(x)
static int l_type(rf_val *fp, int argc) {
    if (!argc)
        return 0;
    char *str;
    size_t len = 0;
    switch (fp->type) {
    case TYPE_NULL: str = "null";     len = 4; break;
    case TYPE_INT:  str = "int";      len = 3; break;
    case TYPE_FLT:  str = "float";    len = 5; break;
    case TYPE_STR:  str = "string";   len = 6; break;
    case TYPE_RE:   str = "regex";    len = 5; break;
    case TYPE_SEQ:  str = "sequence"; len = 8; break;
    case TYPE_TBL:  str = "table";    len = 5; break;
    case TYPE_RFN:
    case TYPE_CFN:  str = "function"; len = 8; break;
    default: break;
    }
    assign_str(fp-1, s_newstr(str, len, 0));
    return 1;
}

// upper(s)
static int l_upper(rf_val *fp, int argc) {
    if (!is_str(fp))
        return 0;
    return allxcase(fp, 1);
}

static struct {
    const char *name;
    c_fn        fn;
} lib_fn[] = {
    // Arithmetic
    { "abs",   { 1, l_abs }   },
    { "atan",  { 1, l_atan }  },
    { "ceil",  { 1, l_ceil }  },
    { "cos",   { 1, l_cos }   },
    { "exp",   { 1, l_exp }   },
    { "int",   { 1, l_int }   },
    { "log",   { 1, l_log }   },
    { "sin",   { 1, l_sin }   },
    { "sqrt",  { 1, l_sqrt }  },
    { "tan",   { 1, l_tan }   },
    // PRNG
    { "rand",  { 0, l_rand }  },
    { "srand", { 0, l_srand } },
    // Strings
    { "byte",  { 1, l_byte }  },
    { "char",  { 0, l_char }  },
    { "fmt",   { 1, l_fmt }   },
    { "gsub",  { 2, l_gsub }  },
    { "hex",   { 1, l_hex }   },
    { "lower", { 1, l_lower } },
    { "num",   { 1, l_num }   },
    { "split", { 1, l_split } },
    { "sub",   { 2, l_sub }   },
    { "type",  { 1, l_type }  },
    { "upper", { 1, l_upper } },
    { NULL,    { 0, NULL }    }
};

void l_register(rf_htbl *g) {
    // Initialize the PRNG with the current time
    srand48(time(0));
    for (int i = 0; lib_fn[i].name; ++i) {
        rf_str *s  = s_newstr(lib_fn[i].name, strlen(lib_fn[i].name), 1);
        rf_val *fn = malloc(sizeof(rf_val));
        *fn        = (rf_val) {TYPE_CFN, .u.cfn = &lib_fn[i].fn};
        h_insert(g, s, fn, 1);
    }
}
