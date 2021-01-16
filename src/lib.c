#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "array.h"
#include "fn.h"
#include "mem.h"
#include "lib.h"

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
        assign_int(fp-1, fp->u.s->str[idx]);
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
// ASCII codes of each respective argument in order
// Ex:
//   char(114, 105, 102, 102) -> "riff"
static int l_char(rf_val *fp, int argc) {
    if (!argc) return 0;
    char buf[argc + 1];
    for (int i = 0; i < argc; ++i) {
        buf[i] = intval(fp+i);
    }
    buf[argc] = '\0';
    assign_str(fp-1, s_newstr(buf, argc, 0));
    return 1;
}

// %c
#define fmt_char(b, n, cap, c, left, width) \
    if (left) { \
        len = snprintf(NULL, 0, "%-*c", width, c); \
        m_resizebuffer(b, n + len + 1, cap, char); \
        snprintf(b + n, len + 1, "%-*c", width, c); \
    } else { \
        len = snprintf(NULL, 0, "%*c", width, c); \
        m_resizebuffer(b, n + len + 1, cap, char); \
        snprintf(b + n, len + 1, "%*c", width, c); \
    }

// %s
#define fmt_str(b, n, cap, s, left, width, prec) \
    if (left) { \
        len = snprintf(NULL, 0, "%-*.*s", width, prec, s); \
        m_resizebuffer(b, n + len + 1, cap, char); \
        snprintf(b + n, len + 1, "%-*.*s", width, prec, s); \
    } else { \
        len = snprintf(NULL, 0, "%*.*s", width, prec, s); \
        m_resizebuffer(b, n + len + 1, cap, char); \
        snprintf(b + n, len + 1, "%*.*s", width, prec, s); \
    }

// Signed fmt conversions: floats, decimal integers
#define fmt_signed(b, n, cap, i, fmt, left, sign, space, zero, width, prec) \
    if (left) { \
        if (sign) { \
            len = snprintf(NULL, 0, "%-+*.*"fmt, width, prec, i); \
            m_resizebuffer(b, n + len + 1, cap, char); \
            snprintf(b + n, len + 1, "%-+*.*"fmt, width, prec, i); \
        } else if (space) { \
            len = snprintf(NULL, 0, "%- *.*"fmt, width, prec, i); \
            m_resizebuffer(b, n + len + 1, cap, char); \
            snprintf(b + n, len + 1, "%- *.*"fmt, width, prec, i); \
        } else { \
            len = snprintf(NULL, 0, "%-*.*"fmt, width, prec, i); \
            m_resizebuffer(b, n + len + 1, cap, char); \
            snprintf(b + n, len + 1, "%-*.*"fmt, width, prec, i); \
        } \
    } else if (zero) { \
        if (sign) { \
            len = snprintf(NULL, 0, "%+0*.*"fmt, width, prec, i); \
            m_resizebuffer(b, n + len + 1, cap, char); \
            snprintf(b + n, len + 1, "%+0*.*"fmt, width, prec, i); \
        } else if (space) { \
            len = snprintf(NULL, 0, "% 0*.*"fmt, width, prec, i); \
            m_resizebuffer(b, n + len + 1, cap, char); \
            snprintf(b + n, len + 1, "% 0*.*"fmt, width, prec, i); \
        } else { \
            len = snprintf(NULL, 0, "%0*.*"fmt, width, prec, i); \
            m_resizebuffer(b, n + len + 1, cap, char); \
            snprintf(b + n, len + 1, "%0*.*"fmt, width, prec, i); \
        } \
    } else if (sign) { \
        len = snprintf(NULL, 0, "%+*.*"fmt, width, prec, i); \
        m_resizebuffer(b, n + len + 1, cap, char); \
        snprintf(b + n, len + 1, "%+*.*"fmt, width, prec, i); \
    } else if (space) { \
        len = snprintf(NULL, 0, "% *.*"fmt, width, prec, i); \
        m_resizebuffer(b, n + len + 1, cap, char); \
        snprintf(b + n, len + 1, "% *.*"fmt, width, prec, i); \
    } else { \
        len = snprintf(NULL, 0, "%*.*"fmt, width, prec, i); \
        m_resizebuffer(b, n + len + 1, cap, char); \
        snprintf(b + n, len + 1, "%*.*"fmt, width, prec, i); \
    }

// Unsigned fmt conversions: hex and octal integers
// Only difference is absence of space flag, since numbers are
// converted to unsigned anyway. clang also throws a warning
// about UB for octal/hex conversions with the space flag.
#define fmt_unsigned(b, n, cap, i, fmt, left, zero, width, prec) \
    if (left) { \
        len = snprintf(NULL, 0, "%-*.*"fmt, width, prec, i); \
        m_resizebuffer(b, n + len + 1, cap, char); \
        snprintf(b + n, len + 1, "%-*.*"fmt, width, prec, i); \
    } else if (zero) { \
        len = snprintf(NULL, 0, "%0*.*"fmt, width, prec, i); \
        m_resizebuffer(b, n + len + 1, cap, char); \
        snprintf(b + n, len + 1, "%0*.*"fmt, width, prec, i); \
    } else { \
        len = snprintf(NULL, 0, "%*.*"fmt, width, prec, i); \
        m_resizebuffer(b, n + len + 1, cap, char); \
        snprintf(b + n, len + 1, "%*.*"fmt, width, prec, i); \
    }

#define DEFAULT_FLT_PREC 6

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
//   f              | Decimal float
//   g              | `e` or `f` conversion (whichever is shorter)
//   o              | Octal integer
//   s              | String
//   x / X          | Hex integer (lowercase/uppercase)
static int l_fmt(rf_val *fp, int argc) {
    if (!is_str(fp))
        return 0;
    --argc;
    int arg = 1;

    const char *fstr = fp->u.s->str;

    char *buf = NULL;
    int   n   = 0;
    int   cap = 0;

    // Flags and specifiers
    int left, sign, space, zero, width, prec;

    while (*fstr && argc) {
        if (*fstr != '%') {
            m_growarray(buf, n, cap, char);
            buf[n++] = *fstr++;
            continue;
        }

        // Advance pointer and check for literal '%'
        if (*++fstr == '%') {
            m_growarray(buf, n, cap, char);
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

        size_t len;
        rf_int i;
        rf_flt f;

        // TODO binary/%b?
        // Evaluate format specifier
        switch (*fstr++) {
        case 'c': {
            if (argc--) {
                int c = (int) intval(fp+arg);
                fmt_char(buf, n, cap, c, left, width);
                ++arg;
            }
            break;
        }
        case 'd': case 'i': {
            if (argc--) {
redir_int:
                i = intval(fp+arg);
                fmt_signed(buf, n, cap, i, PRId64, left, sign, space, zero, width, prec);
                ++arg;
            }
            break;
        }
        case 'o':
            if (argc--) {
                i = intval(fp+arg);
                fmt_unsigned(buf, n, cap, i, PRIo64, left, zero, width, prec);
                ++arg;
            }
            break;
        case 'x':
            if (argc--) {
                i = intval(fp+arg);
                fmt_unsigned(buf, n, cap, i, PRIx64, left, zero, width, prec);
                ++arg;
            }
            break;
        case 'X':
            if (argc--) {
                i = intval(fp+arg);
                fmt_unsigned(buf, n, cap, i, PRIX64, left, zero, width, prec);
                ++arg;
            }
            break;
        case 'a':
            if (argc--) {
                f = fltval(fp+arg);
                // Default precision left as -1 for `a`
                fmt_signed(buf, n, cap, f, "a", left, sign, space, zero, width, prec);
                ++arg;
            }
            break;
        case 'A':
            if (argc--) {
                f = fltval(fp+arg);
                // Default precision left as -1 for `A`
                fmt_signed(buf, n, cap, f, "A", left, sign, space, zero, width, prec);
                ++arg;
            }
            break;
        case 'e':
            if (argc--) {
                f = fltval(fp+arg);
                prec = prec < 0 ? DEFAULT_FLT_PREC : prec;
                fmt_signed(buf, n, cap, f, "e", left, sign, space, zero, width, prec);
                ++arg;
            }
            break;
        case 'E':
            if (argc--) {
                f = fltval(fp+arg);
                prec = prec < 0 ? DEFAULT_FLT_PREC : prec;
                fmt_signed(buf, n, cap, f, "E", left, sign, space, zero, width, prec);
                ++arg;
            }
            break;
        case 'f':
            if (argc--) {
                f = fltval(fp+arg);
                prec = prec < 0 ? DEFAULT_FLT_PREC : prec;
                fmt_signed(buf, n, cap, f, "f", left, sign, space, zero, width, prec);
                ++arg;
            }
            break;
        case 'g':
            if (argc--) {
redir_flt:
                f = fltval(fp+arg);
                prec = prec < 0 ? DEFAULT_FLT_PREC : prec;
                fmt_signed(buf, n, cap, f, "g", left, sign, space, zero, width, prec);
                ++arg;
            }
            break;

        // %s should accept any type; redirect as needed
        case 's':
            if (argc--) {
                if (is_str(fp+arg)) {
                    fmt_str(buf, n, cap, fp[arg].u.s->str, left, width, prec);
                } else if (is_int(fp+arg)) {
                    goto redir_int;
                } else if (is_flt(fp+arg)) {
                    goto redir_flt;
                }
                ++arg;
            }
            break;
        default:
            // Throw error
            fprintf(stderr, "riff: [fmt] invalid format specifier: %c\n", fstr[-1]);
            exit(1);
        }
        n += len;
    }

    // Copy rest of string after exhausting user-provided args
    while (*fstr) {
        m_growarray(buf, n, cap, char);
        buf[n++] = *fstr++;
    }

    assign_str(fp-1, s_newstr(buf, n, 0));
    if (buf)
        free(buf);
    return 1;
}

// hex(x)
// Returns a string of `x` as an integer in hexadecimal (lowercase)
// with the leading "0x"
// Ex:
//   hex(255) -> "0xff"
static int l_hex(rf_val *fp, int argc) {
    rf_int i = intval(fp);
    size_t len = snprintf(NULL, 0, "0x%"PRIx64, i);
    char buf[len + 1];
    snprintf(buf, len + 1, "0x%"PRIx64, i);
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
//
// TODO num() should be able to read any type of numerical constants
// defined by the language itself, such as floats or binary integers
// with the leading `0b`.
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

    // If the provided base is outside the range, default back to 0
    if (base < 2 || base > 36)
        base = 0;
    char *end;
    assign_int(fp-1, (rf_int) strtoll(fp->u.s->str, &end, base));
    return 1;
}

// split(s[,d])
// Returns an array with elements being string `s` split on delimiter
// `d`. The delimiter can be zero or more characters. If no delimiter
// is provided, the default is " \t". If the delimiter is the empty
// string (""), the string is split into an array of individual
// characters.
static int l_split(rf_val *fp, int argc) {
    if (!is_str(fp))
        return 0;
    size_t len = fp->u.s->l + 1;
    char str[len];
    memcpy(str, fp->u.s->str, len);
    const char *delim = (argc < 2 || !is_str(fp+1)) ? " \t" : fp[1].u.s->str;
    rf_val *arr = v_newarr();
    rf_str *s;
    rf_val v;
    if (!strlen(delim)) {
        for (rf_int i = 0; i < len - 1; ++i) {
            s = s_newstr(str + i, 1, 0);
            v = (rf_val) {TYPE_STR, .u.s = s};
            a_insert_int(arr->u.a, i, &v, 1, 1);
        }
    } else {
        char *tk = strtok(str, delim);
        for (rf_int i = 0; tk; ++i) {
            s = s_newstr(tk, strlen(tk), 0);
            v = (rf_val) {TYPE_STR, .u.s = s};
            a_insert_int(arr->u.a, i, &v, 1, 1);
            tk = strtok(NULL, delim);
        }
    }
    fp[-1] = *arr;
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
    { "hex",   { 1, l_hex }   },
    { "lower", { 1, l_lower } },
    { "num",   { 1, l_num }   },
    { "split", { 1, l_split } },
    { "upper", { 1, l_upper } },
    { NULL,    { 0, NULL }    }
};

void l_register(hash_t *g) {
    // Initialize the PRNG with the current time
    srand48(time(0));
    for (int i = 0; lib_fn[i].name; ++i) {
        rf_str *s  = s_newstr(lib_fn[i].name, strlen(lib_fn[i].name), 1);
        rf_val *fn = malloc(sizeof(rf_val));
        *fn        = (rf_val) {TYPE_CFN, .u.cfn = &lib_fn[i].fn};
        h_insert(g, s, fn, 1);
    }
}
