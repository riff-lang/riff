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
RIFF_LIB_FN(abs) {
    if (is_int(fp))
        set_int(fp-1, llabs(fp->u.i));
    else
        set_flt(fp-1, fabs(fltval(fp)));
    return 1;
}

// atan(y[,x])
RIFF_LIB_FN(atan) {
    if (argc == 1)
        set_flt(fp-1, atan(fltval(fp)));
    else if (argc > 1)
        set_flt(fp-1, atan2(fltval(fp), fltval(fp+1)));
    return 1;
}

// ceil(x)
RIFF_LIB_FN(ceil) {
    set_int(fp-1, (rf_int) ceil(fltval(fp)));
    return 1;
}

// cos(x)
RIFF_LIB_FN(cos) {
    set_flt(fp-1, cos(fltval(fp)));
    return 1;
}

// exp(x)
RIFF_LIB_FN(exp) {
    set_flt(fp-1, exp(fltval(fp)));
    return 1;
}

// int(x)
RIFF_LIB_FN(int) {
    set_int(fp-1, intval(fp));
    return 1;
}

// log(x[,b])
RIFF_LIB_FN(log) {
    if (argc == 1)
        set_flt(fp-1, log(fltval(fp)));
    else if (fltval(fp+1) == 2.0)
        set_flt(fp-1, log2(fltval(fp)));
    else if (fltval(fp+1) == 10.0)
        set_flt(fp-1, log10(fltval(fp)));
    else
        set_flt(fp-1, log(fltval(fp)) / log(fltval(fp+1)));
    return 1;
}

// sin(x)
RIFF_LIB_FN(sin) {
    set_flt(fp-1, sin(fltval(fp)));
    return 1;
}

// sqrt(x)
RIFF_LIB_FN(sqrt) {
    set_flt(fp-1, sqrt(fltval(fp)));
    return 1;
}

// tan(x)
RIFF_LIB_FN(tan) {
    set_flt(fp-1, tan(fltval(fp)));
    return 1;
}

// Pseudo-random number generation

// xoshiro256**
// Source: https://prng.di.unimi.it

static inline rf_uint rol(const rf_uint x, int k) {
    return (x << k) | (x >> (64 - k));
}

static rf_uint prngs[4];

static rf_uint prng_next(void) {
    const rf_uint res = rol(prngs[1] * 5, 7) * 9;
    const rf_uint t   = prngs[1] << 17;

    prngs[2] ^= prngs[0];
    prngs[3] ^= prngs[1];
    prngs[1] ^= prngs[2];
    prngs[0] ^= prngs[3];
    prngs[2] ^= t;
    prngs[3]  = rol(prngs[3], 45);

    return res;
}

// rand([x])
//   rand()     | random float ∈ [0..1)
//   rand(0)    | random int ∈ [INT64_MIN..INT64_MAX]
//   rand(n)    | random int ∈ [0..n]
//   rand(m,n)  | random int ∈ [m..n]
//   rand(seq)  | random int ∈ (range/sequence)
RIFF_LIB_FN(rand) {
    rf_uint rand = prng_next();
    if (!argc) {
        rf_flt f = (rf_flt) ((rand >> 11) * (0.5 / ((rf_uint)1 << 52)));
        set_flt(fp-1, f);
    }

    // If first argument is a range/sequence, ignore any succeeding
    // args
    else if (is_seq(fp)) {
        rf_int from = fp->u.q->from;
        rf_int to   = fp->u.q->to;
        rf_int itvl = fp->u.q->itvl;
        rf_uint range, offset;
        if (from < to) {
            //           <<<
            // [i64min..from] ∪ [to..i64max]
            if (itvl < 0) {
                range  = UINT64_MAX - (to - from) + 1;
                itvl   = llabs(itvl);
                offset = to + (range % itvl);
            } else {
                range  = to - from;
                offset = from;
            }
        }

        //                 >>>
        // [i64min..to] ∪ [from..i64max]
        else if (from > to) {
            if (itvl > 0) {
                range  = UINT64_MAX - (from - to) + 1;
                offset = from;
            } else {
                range  = from - to;
                itvl   = llabs(itvl);
                offset = to + (range % itvl);
            }
        } else {
            set_int(fp-1, from);
            return 1;
        }
        range /= itvl;
        range += !(range == UINT64_MAX);
        rand  %= range;
        rand  *= itvl;
        rand  += offset;
        set_int(fp-1, rand);
    }

    // 1 argument (0..n)
    else if (argc == 1) {
        rf_int n1 = intval(fp);
        if (n1 == 0) {
            set_int(fp-1, rand);
        } else {
            if (n1 > 0) {
                set_int(fp-1,   rand %  (n1 + 1));
            } else {
                set_int(fp-1, -(rand % -(n1 - 1)));
            }
        }
    }

    // 2 arguments (m..n)
    else {
        rf_int n1 = intval(fp);
        rf_int n2 = intval(fp+1);
        rf_uint range, offset;
        if (n1 < n2) {
            range  = n2 - n1;
            offset = n1;
        } else if (n1 > n2) {
            range  = n1 - n2;
            offset = n2;
        } else {
            set_int(fp-1, n1);
            return 1;
        }
        range += !(range == UINT64_MAX);
        set_int(fp-1, (rand % range) + offset);
    }
    return 1;
}

// I believe this is how you would utilize splitmix64 to initialize
// the PRNG state
static void prng_seed(rf_uint seed) {
    prngs[0] = seed + 0x9e3779b97f4a7c15u;
    prngs[1] = (prngs[0] ^ (prngs[0] >> 30)) * 0xbf58476d1ce4e5b9u;
    prngs[2] = (prngs[1] ^ (prngs[1] >> 27)) * 0x94d049bb133111ebu;
    prngs[3] =  prngs[2] ^ (prngs[2] >> 31);
    for (int i = 0; i < 16; ++i)
        prng_next();
}

// srand([x])
// Initializes the PRNG with seed `x` or time(0) if no argument given.
// rand() will produce the same sequence when srand is initialized
// with a given seed every time.
RIFF_LIB_FN(srand) {
    if (!argc)
        prng_seed(time(0));
    else if (is_null(fp))
        prng_seed(0);
    else
        // Seed the PRNG with whatever 64 bits are in the rf_val union
        prng_seed(fp->u.i);
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
RIFF_LIB_FN(byte) {
    int idx = argc > 1 ? intval(fp+1) : 0;
    if (is_str(fp)) {
        if (idx > fp->u.s->l)
            idx = fp->u.s->l;
        set_int(fp-1, (uint8_t) fp->u.s->str[idx]);
    } else if (is_rfn(fp)) {
        if (idx > fp->u.fn->code->n)
            idx = fp->u.fn->code->n;
        set_int(fp-1, fp->u.fn->code->code[idx]);
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
RIFF_LIB_FN(char) {
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
    set_str(fp-1, s_newstr(buf, n, 0));
    return 1;
}

#define FMT_ZERO  1
#define FMT_SIGN  2
#define FMT_SPACE 4
#define FMT_LEFT  8

// %c
#define fmt_char(b, n, c) \
    if (flags & FMT_LEFT) { \
        n += sprintf(b + n, "%-*c", width, c); \
    } else { \
        n += sprintf(b + n, "%*c", width, c); \
    }

// %s
#define fmt_str(b, n, s) \
    if (flags & FMT_LEFT) { \
        n += sprintf(b + n, "%-*.*s", width, prec, s); \
    } else { \
        n += sprintf(b + n, "%*.*s", width, prec, s); \
    }

// Signed fmt conversions: floats, decimal integers
// NOTE: gcc complains about `0` flag with precision
#define fmt_signed(b, n, i, fmt) \
    if (flags & FMT_LEFT) { \
        if (flags & FMT_SIGN) { \
            n += sprintf(b + n, "%-+*.*"fmt, width, prec, i); \
        } else if (flags & FMT_SPACE) { \
            n += sprintf(b + n, "%- *.*"fmt, width, prec, i); \
        } else { \
            n += sprintf(b + n, "%-*.*"fmt, width, prec, i); \
        } \
    } else if ((prec < 0) && (flags & FMT_ZERO)) { \
        if (flags & FMT_SIGN) { \
            n += sprintf(b + n, "%+0*"fmt, width, i); \
        } else if (flags & FMT_SPACE) { \
            n += sprintf(b + n, "% 0*"fmt, width, i); \
        } else { \
            n += sprintf(b + n, "%0*"fmt, width, i); \
        } \
    } else if (flags & FMT_SIGN) { \
        n += sprintf(b + n, "%+*.*"fmt, width, prec, i); \
    } else if (flags & FMT_SPACE) { \
        n += sprintf(b + n, "% *.*"fmt, width, prec, i); \
    } else { \
        n += sprintf(b + n, "%*.*"fmt, width, prec, i); \
    }

// Unsigned fmt conversions: hex and octal integers
// Only difference is absence of space flag, since numbers are
// converted to unsigned anyway. clang also throws a warning
// about UB for octal/hex conversions with the space flag.
#define fmt_unsigned(b, n, i, fmt) \
    if (flags & FMT_LEFT) { \
        n += sprintf(b + n, "%-*.*"fmt, width, prec, i); \
    } else if ((prec < 0) && (flags & FMT_ZERO)) { \
        n += sprintf(b + n, "%0*"fmt, width, i); \
    } else { \
        n += sprintf(b + n, "%*.*"fmt, width, prec, i); \
    }

// %b
static int fmt_bin_itoa(char *buf, rf_int num, unsigned int flags, int width, int prec) {

    if (!num && !prec)
        return 0;

    int  size = width > 64 ? width : 64;
    char temp[size];
    int  len = 0;
    do {
        temp[size-(++len)] = '0' + (num & 1);
        num >>= 1;
    } while (num && len < size);

    while (len < prec) {
        temp[size-(++len)] = '0';
    }

    if (!(flags & FMT_LEFT)) {
        char padc = flags & FMT_ZERO ? '0' : ' ';
        while (len < width) {
            temp[size-(++len)] = padc;
        }
        memcpy(buf, temp + (size-len), len);
    } else {
        memcpy(buf, temp + (size-len), len);
        if (len < width) {
            memset(buf + len, ' ', width - len);
            len = width;
        }
    }
    return len;
}

// fmt(...)
// Riff's `sprintf()` implementation. Doubles as `printf()` due to the
// implicit printing functionality of the language.
//
// Optional modifiers (zero or more):
//   +              | Prepend with sign
//   -              | Left-justified
//   <space>        | Space-padded (ignored if left-justified)
//   0              | Zero-padded (ignored if left-justified)
//   <integer> / *  | Field width
//   .              | Precision specifier
//
// Format specifiers:
//   %              | Literal `%`
//   a / A          | Hex float (exp notation; lowercase/uppercase)
//   b              | Binary integer
//   c              | Single character
//   d / i          | Decimal integer
//   e / E          | Decimal float (exp notation)
//   f / F          | Decimal float
//   g              | `e` or `f` conversion (whichever is shorter)
//   G              | `E` or `F` conversion (whichever is shorter)
//   o              | Octal integer
//   s              | String
//   x / X          | Hex integer (lowercase/uppercase)
RIFF_LIB_FN(fmt) {
    if (!is_str(fp))
        return 0;
    --argc;
    int arg = 1;

    const char *fstr = fp->u.s->str;

    char buf[STR_BUF_SZ];
    int  n = 0;

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

        // Flags and specifiers
        unsigned int flags = 0;
        int width = -1;

        // Both clang and gcc seem to allow -1 to be used as a
        // precision modifier without throwing warnings, so this is a
        // useful default
        int prec = -1;

capture_flags:
        switch (*fstr) {
        case '0': flags |= FMT_ZERO;  ++fstr; goto capture_flags;
        case '+': flags |= FMT_SIGN;  ++fstr; goto capture_flags;
        case ' ': flags |= FMT_SPACE; ++fstr; goto capture_flags;
        case '-': flags |= FMT_LEFT;  ++fstr; goto capture_flags;
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

        // Evaluate format specifier
        switch (*fstr++) {
        case 'c': {
            if (argc--) {
                int c = (int) intval(fp+arg);
                fmt_char(buf, n, c);
                ++arg;
            }
            break;
        }
        case 'd': case 'i': {
            if (argc--) {
redir_int:
                i = intval(fp+arg);
                fmt_signed(buf, n, i, PRId64);
                ++arg;
            }
            break;
        }
        case 'o':
            if (argc--) {
                i = intval(fp+arg);
                fmt_unsigned(buf, n, i, PRIo64);
                ++arg;
            }
            break;
        case 'x':
            if (argc--) {
                i = intval(fp+arg);
                fmt_unsigned(buf, n, i, PRIx64);
                ++arg;
            }
            break;
        case 'X':
            if (argc--) {
                i = intval(fp+arg);
                fmt_unsigned(buf, n, i, PRIX64);
                ++arg;
            }
            break;
        case 'b':
            if (argc--) {
                n += fmt_bin_itoa(buf + n, intval(fp+arg), flags, width, prec);
                ++arg;
            }
            break;
        case 'a':
            if (argc--) {
                f = fltval(fp+arg);
                // Default precision left as -1 for `a`
                fmt_signed(buf, n, f, "a");
                ++arg;
            }
            break;
        case 'A':
            if (argc--) {
                f = fltval(fp+arg);
                // Default precision left as -1 for `A`
                fmt_signed(buf, n, f, "A");
                ++arg;
            }
            break;
        case 'e':
            if (argc--) {
                f = fltval(fp+arg);
                prec = prec < 0 ? DEFAULT_FLT_PREC : prec;
                fmt_signed(buf, n, f, "e");
                ++arg;
            }
            break;
        case 'E':
            if (argc--) {
                f = fltval(fp+arg);
                prec = prec < 0 ? DEFAULT_FLT_PREC : prec;
                fmt_signed(buf, n, f, "E");
                ++arg;
            }
            break;
        case 'f': case 'F':
            if (argc--) {
                f = fltval(fp+arg);
                prec = prec < 0 ? DEFAULT_FLT_PREC : prec;
                fmt_signed(buf, n, f, "f");
                ++arg;
            }
            break;
        case 'g':
            if (argc--) {
redir_flt:
                f = fltval(fp+arg);
                prec = prec < 0 ? DEFAULT_FLT_PREC : prec;
                fmt_signed(buf, n, f, "g");
                ++arg;
            }
            break;
        case 'G':
            if (argc--) {
                f = fltval(fp+arg);
                prec = prec < 0 ? DEFAULT_FLT_PREC : prec;
                fmt_signed(buf, n, f, "G");
                ++arg;
            }
            break;

        // %s should accept any type; redirect as needed
        case 's':
            if (argc--) {
                if (is_str(fp+arg)) {
                    fmt_str(buf, n, fp[arg].u.s->str);
                } else if (is_int(fp+arg)) {
                    goto redir_int;
                } else if (is_flt(fp+arg)) {
                    goto redir_flt;
                }

                // TODO handle other types
                else {
                    fmt_str(buf, n, "");
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

    set_str(fp-1, s_newstr(buf, n, 0));
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
    set_str(fp-1, s_newstr(buf, n, 0));
    return 1;
}

// gsub(s,p[,r])
// Returns a copy of string `s` where all occurrences of pattern `p`
// are replaced by string `r`
RIFF_LIB_FN(gsub) {
    return xsub(fp, argc, PCRE2_SUBSTITUTE_GLOBAL);
}

// hex(x)
// Returns a string of `x` as an integer in hexadecimal (lowercase)
// with the leading "0x"
// Ex:
//   hex(255) -> "0xff"
RIFF_LIB_FN(hex) {
    rf_int i = intval(fp);
    char buf[20];
    int len = sprintf(buf, "0x%"PRIx64, i);
    set_str(fp-1, s_newstr(buf, len, 0));
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
    set_str(fp-1, s_newstr(str, len, 0));
    return 1;
}

// lower(s)
RIFF_LIB_FN(lower) {
    if (!is_str(fp))
        return 0;
    return allxcase(fp, 0);
}

// num(s[,b])
// Takes a string `s` and an optional base `b` and returns the
// interpreted num. The base can be 0 or an integer in the range
// {2..36}. The default base is 0. This function is more or less a
// direct interface to `strtoll()`.
RIFF_LIB_FN(num) {
    if (!is_str(fp)) {
        if (is_int(fp)) {
            set_int(fp-1, fp->u.i);
        } else if (is_flt(fp)) {
            set_flt(fp-1, fp->u.f);
        } else {
            set_int(fp-1, 0);
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
    set_int(fp-1, i);
    return 1;
ret_flt:
    set_flt(fp-1, u_str2d(fp->u.s->str, &end, base));
    return 1;
}

// split(s[,d])
// Returns a table with elements being string `s` split on delimiter
// `d`, treated as a regular expression. If no delimiter is provided,
// the regular expression /\s+/ (whitespace) is used. If the delimiter
// is the empty string (""), the string is split into a table of
// single-byte strings.
RIFF_LIB_FN(split) {
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
RIFF_LIB_FN(sub) {
    return xsub(fp, argc, 0);
}

// type(x)
RIFF_LIB_FN(type) {
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
    set_str(fp-1, s_newstr(str, len, 0));
    return 1;
}

// upper(s)
RIFF_LIB_FN(upper) {
    if (!is_str(fp))
        return 0;
    return allxcase(fp, 1);
}

static struct {
    const char *name;
    c_fn        fn;
} lib_fn[] = {
    // Arithmetic
    RIFF_LIB_REG(abs,   1),
    RIFF_LIB_REG(atan,  1),
    RIFF_LIB_REG(ceil,  1),
    RIFF_LIB_REG(cos,   1),
    RIFF_LIB_REG(exp,   1),
    RIFF_LIB_REG(int,   1),
    RIFF_LIB_REG(log,   1),
    RIFF_LIB_REG(sin,   1),
    RIFF_LIB_REG(sqrt,  1),
    RIFF_LIB_REG(tan,   1),
    // PRNG
    RIFF_LIB_REG(rand,  0),
    RIFF_LIB_REG(srand, 0),
    // Strings
    RIFF_LIB_REG(byte,  1),
    RIFF_LIB_REG(char,  0),
    RIFF_LIB_REG(fmt,   1),
    RIFF_LIB_REG(gsub,  2),
    RIFF_LIB_REG(hex,   1),
    RIFF_LIB_REG(lower, 1),
    RIFF_LIB_REG(num,   1),
    RIFF_LIB_REG(split, 1),
    RIFF_LIB_REG(sub,   2),
    RIFF_LIB_REG(type,  1),
    RIFF_LIB_REG(upper, 1),
    { NULL, { 0, NULL } }
};

void l_register(rf_htbl *g) {
    // Initialize the PRNG with the current time
    prng_seed(time(0));
    for (int i = 0; lib_fn[i].name; ++i) {
        rf_str *s  = s_newstr(lib_fn[i].name, strlen(lib_fn[i].name), 1);
        rf_val *fn = malloc(sizeof(rf_val));
        *fn        = (rf_val) {TYPE_CFN, .u.cfn = &lib_fn[i].fn};
        h_insert(g, s, fn, 1);
    }
}
