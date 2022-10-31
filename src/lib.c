#include "lib.h"

#include "buf.h"
#include "conf.h"
#include "state.h"
#include "fmt.h"
#include "fn.h"
#include "mem.h"
#include "parse.h"
#include "prng.h"
#include "string.h"
#include "vm.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Dedicated user PRNG state
static riff_prng_state prngs;

static void err(const char *msg) {
    fprintf(stderr, "riff: %s\n", msg);
    exit(1);
}

// Common type signature for library functions
#define LIB_FN(name) static int l_##name(riff_val *fp, int argc)

// Basic arithmetic functions
// Strict arity of 1 where argument is uncondtionally coerced.
LIB_FN(ceil) { set_int(fp-1, (riff_int) ceil(fltval(fp))); return 1; }
LIB_FN(cos)  { set_flt(fp-1, cos(fltval(fp)));           return 1; }
LIB_FN(exp)  { set_flt(fp-1, exp(fltval(fp)));           return 1; }
LIB_FN(int)  { set_int(fp-1, intval(fp));                return 1; }
LIB_FN(sin)  { set_flt(fp-1, sin(fltval(fp)));           return 1; }
LIB_FN(sqrt) { set_flt(fp-1, sqrt(fltval(fp)));          return 1; }
LIB_FN(tan)  { set_flt(fp-1, tan(fltval(fp)));           return 1; }

// abs(x)
LIB_FN(abs) {
    if (is_int(fp)) {
        set_int(fp-1, llabs(fp->i));
    } else {
        set_flt(fp-1, fabs(fltval(fp)));
    }
    return 1;
}

// atan(y[,x])
LIB_FN(atan) {
    if (argc == 1) {
        set_flt(fp-1, atan(fltval(fp)));
    } else if (argc > 1) {
        set_flt(fp-1, atan2(fltval(fp), fltval(fp+1)));
    }
    return 1;
}

// log(x[,b])
LIB_FN(log) {
    if (argc == 1) {
        set_flt(fp-1, log(fltval(fp)));
    } else if (fltval(fp+1) == 2.0) {
        set_flt(fp-1, log2(fltval(fp)));
    } else if (fltval(fp+1) == 10.0) {
        set_flt(fp-1, log10(fltval(fp)));
    } else {
        set_flt(fp-1, log(fltval(fp)) / log(fltval(fp+1)));
    }
    return 1;
}

// I/O functions

// close(f)
LIB_FN(close) {
    if (is_file(fp))
        if (!(fp->fh->flags & FH_STD))
            fclose(fp->fh->p);
    return 0;
}

// eof(f)
LIB_FN(eof) {
    set_int(fp-1, !is_file(fp) || feof(fp->fh->p));
    return 1;
}

// eval(s)
LIB_FN(eval) {
    if (!is_str(fp)) {
        return 0;
    }
    riff_state s;
    riff_fn main;
    riff_code c;

    riff_state_init(&s);
    c_init(&c);
    main.code = &c;
    main.arity = 0;
    s.main = main;
    s.src = fp->s->str;
    main.name = NULL;
    riff_compile(&s);
    vm_exec_reenter(&s, (vm_stack *) fp);
    return 0;
}

// flush([f])
LIB_FN(flush) {
    FILE *f = argc && is_file(fp) ? fp->fh->p : stdout;
    if (fflush(f)) {
        err("error flushing stream");
    }
    return 0;
}

// get([n])
LIB_FN(get) {
    char buf[STR_BUF_SZ];
    if (argc) {
        size_t count = intval(fp);
        size_t nread = fread(buf, sizeof *buf, count, stdin);
        set_str(fp-1, riff_str_new(buf, nread));
        return 1;
    }
    if (!fgets(buf, sizeof buf, stdin)) {
        return 0;
    }
    set_str(fp-1, riff_str_new(buf, strcspn(buf, "\n")));
    return 1;
}

// getc([f])
LIB_FN(getc) {
    riff_int c;
    FILE *f = argc && is_file(fp) ? fp->fh->p : stdin;
    if ((c = fgetc(f)) != EOF) {
        set_int(fp-1, c);
        return 1;
    }
    return 0;
}


static int valid_fmode(char *mode) {
    return (*mode && memchr("rwa", *(mode++), 3) &&
           (*mode != '+' || ((void)(++mode), 1)) &&
           (strspn(mode, "b") == strlen(mode)));
}

// open(s[,m])
LIB_FN(open) {
    if (!is_str(fp))
        return 0;
    FILE *p;
    errno = 0;
    if (argc == 1 || !is_str(fp+1)) {
        p = fopen(fp[0].s->str, "r");
    } else {
        if (!valid_fmode(fp[1].s->str)) {
            fprintf(stderr, "riff: error opening '%s': invalid file mode: '%s'\n",
                    fp[0].s->str, fp[1].s->str);
            exit(1);
        }
        p = fopen(fp[0].s->str, fp[1].s->str);
    }
    if (!p) {
        fprintf(stderr, "riff: error opening '%s': %s\n",
                fp[0].s->str, strerror(errno));
        exit(1);
    }
    riff_file *fh = malloc(sizeof(riff_file));
    fh->p = p;
    fh->flags = 0;
    fp[-1] = (riff_val) {TYPE_FILE, .fh = fh};
    return 1;
}

static inline void fputs_val(FILE *f, riff_val *v) {
    char buf[STR_BUF_SZ];
    char *p = buf;
    riff_tostr(v, &p);
    fputs(p, f);
}

// print(...)
LIB_FN(print) {
    for (int i = 0; i < argc; ++i) {
        if (i) fputs(" ", stdout);
        fputs_val(stdout, fp+i);
    }
    fputs("\n", stdout);
    set_int(fp-1, argc);
    return 1;
}

LIB_FN(write);

// printf(s, ...)
LIB_FN(printf) {
    if (!is_str(fp)) {
        return l_write(fp, argc);
    }
    --argc;
    char buf[STR_BUF_SZ];
    int n = fmt_snprintf(buf, sizeof buf, fp->s->str, fp + 1, argc);
    buf[n] = '\0';
    fputs(buf, stdout);
    return 0;
}

static int build_char_str(riff_val *fp, int argc, char *buf) {
    int n = 0;
    for (int i = 0; i < argc; ++i) {
        uint32_t c = (uint32_t) intval(fp+i);
        if (c <= 0x7f) {
            buf[n++] = (char) c;
        } else {
            char ubuf[8];
            int j = 8 - riff_unicodetoutf8(ubuf, c);
            for (; j < 8; ++j) {
                buf[n++] = ubuf[j];
            }
        }
    }
    return n;
}

// putc(...)
// Takes zero or more integers and prints a string composed of the
// character codes of each respective argument in order
// Ex:
//   putc(114, 105, 102, 102) -> "riff"
LIB_FN(putc) {
    if (UNLIKELY(!argc))
        return 0;
    char buf[STR_BUF_SZ];
    int n = build_char_str(fp, argc, buf);
    buf[n] = '\0';
    fputs(buf, stdout);
    set_int(fp-1, n);
    return 1;
}

#define READ_BUF_SZ 0x10000

static inline int read_bytes(FILE *f, riff_int n, riff_str **ret) {
    if (n) {
        riff_buf buf;
        riff_buf_init_size(&buf, n);
        size_t nr = fread(buf.buf, sizeof (char), n, f);
        *ret = riff_str_new(buf.buf, nr);
        riff_buf_free(&buf);
        return nr > 0;
    } else {
        int c = getc(f);
        ungetc(c, f);
        *ret = riff_str_new("", 0);
        return c != EOF;
    }
}

static inline int read_line(FILE *f, riff_str **ret) {
    riff_buf buf;
    size_t m = READ_BUF_SZ;
    riff_buf_init_size(&buf, m);
    while (1) {
        riff_buf_resize(&buf, m);
        if (fgets(buf.buf + buf.n, m - buf.n, f) == NULL) {
            break;
        }
        buf.n += (size_t) strlen(buf.buf + buf.n);
        if (buf.n && buf.buf[buf.n-1] == '\n') {
            --buf.n;
            break;
        }
        if (buf.n >= m - 64) {
            m += m;
        }
    }
    *ret = riff_str_new(buf.buf, buf.n);
    riff_buf_free(&buf);
    return 1;
}

static inline int read_all(FILE *f, riff_str **ret) {
    riff_buf buf;
    size_t m = READ_BUF_SZ;
    riff_buf_init_size(&buf, m);
    do {
        riff_buf_resize(&buf, m);
        buf.n += fread(buf.buf + buf.n, sizeof (char), m - buf.n, f);
        m += m;
    } while (buf.n == m);
    *ret = riff_str_new(buf.buf, buf.n);
    riff_buf_free(&buf);
    return 1;
}

static inline int read_file_mode(FILE *f, char *mode, riff_str **ret) {
    switch (*mode) {
    case 'A':
    case 'a':
        return read_all(f, ret);
    case 'L':
    case 'l':
    default:
        return read_line(f, ret);
    }
}

// read([a[,b]])
LIB_FN(read) {
    riff_str *ret = NULL;
    int res = 0;
    if (!argc) {
        read_line(stdin, &ret);
    } else if (!is_file(fp)) {
        if (is_str(fp)) {
            res = read_file_mode(stdin, fp->s->str, &ret);
        } else {
            res = read_bytes(stdin, intval(fp), &ret);
        }
    } else if (is_file(fp)) {
        if (argc == 1) {
            res = read_line(fp->fh->p, &ret);
        } else {
            if (is_str(fp+1)) {
                res = read_file_mode(fp->fh->p, fp[1].s->str, &ret);
            } else {
                res = read_bytes(fp->fh->p, intval(fp+1), &ret);
            }
        }
    }

    if (UNLIKELY(!res)) {
        set_int(fp-1, 0);
    } else {
        set_str(fp-1, ret);
    }
    return 1;
}

// write(v[,f])
LIB_FN(write) {
    if (UNLIKELY(!argc)) {
        return 0;
    }
    FILE *f = argc > 1 && is_file(fp+1) ? fp[1].fh->p : stdout;
    fputs_val(f, fp);
    return 0;
}

// PRNG functions

// rand([x])
//   rand()       | random float ∈ [0..1)
//   rand(0)      | random int ∈ [INT64_MIN..INT64_MAX]
//   rand(n)      | random int ∈ [0..n]
//   rand(m,n)    | random int ∈ [m..n]
//   rand(range)  | random int ∈ range
LIB_FN(rand) {
    riff_uint rand = riff_prng_next(&prngs);
    if (!argc) {
        riff_float f = (riff_float) ((rand >> 11) * (0.5 / ((riff_uint)1 << 52)));
        set_flt(fp-1, f);
    }

    // If first argument is a range, ignore any succeeding args
    else if (is_range(fp)) {
        riff_int from = fp->q->from;
        riff_int to   = fp->q->to;
        riff_int itvl = fp->q->itvl;
        riff_uint range, offset;
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
        riff_int n1 = intval(fp);
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
        riff_int n1 = intval(fp);
        riff_int n2 = intval(fp+1);
        riff_uint range, offset;
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

// srand([x])
// Initializes the PRNG with seed `x` or time(0) if no argument given.
// rand() will produce the same sequence when srand is initialized
// with a given seed every time.
LIB_FN(srand) {
    riff_int seed = 0;
    if (!argc)
        seed = time(0);
    else if (!is_null(fp))
        // Seed the PRNG with whatever 64 bits are in the riff_val union
        seed = fp->i;
    riff_prng_seed(&prngs, seed);
    set_int(fp-1, seed);
    return 1;
}

// String functions

// byte(s[,i])
// Takes one string and an optional index argument `i` and returns the
// numeric ASCII code associated with that character. The default
// value for `i` is 0.
LIB_FN(byte) {
    int idx = argc > 1 ? intval(fp+1) : 0;
    if (is_str(fp)) {
        if (idx > riff_strlen(fp->s))
            idx = riff_strlen(fp->s);
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
    if (!argc)
        return 0;
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

// num(s[,b])
// Takes a string `s` and an optional base `b` and returns the
// interpreted num. The base can be 0 or an integer in the range
// {2..36}. The default base is 0. This function is more or less a
// direct interface to `strtoll()`.
LIB_FN(num) {
    if (!is_str(fp)) {
        if (is_int(fp)) {
            set_int(fp-1, fp->i);
        } else if (is_float(fp)) {
            set_flt(fp-1, fp->f);
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
    riff_int i = riff_strtoll(fp->s->str, &end, base);
    if (errno == ERANGE || isdigit(*end)) {
        goto ret_flt;
    }
    if (*end == '.') {
        if ((base == 10 && isdigit(end[1])) ||
            (base == 16 && isxdigit(end[1]))) {
            goto ret_flt;
        }
    } else if (base == 10 && (*end == 'e' || *end == 'E')) {
        goto ret_flt;
    } else if (base == 16 && (*end == 'p' || *end == 'P')) {
        goto ret_flt;
    }
    set_int(fp-1, i);
    return 1;
ret_flt:
    set_flt(fp-1, riff_strtod(fp->s->str, &end, base));
    return 1;
}

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

// type(x)
LIB_FN(type) {
    if (!argc)
        return 0;
    char *str;
    size_t len = 0;
    switch (fp->type) {
    case TYPE_NULL:  str = "null";     len = 4; break;
    case TYPE_INT:   str = "int";      len = 3; break;
    case TYPE_FLOAT: str = "float";    len = 5; break;
    case TYPE_STR:   str = "string";   len = 6; break;
    case TYPE_REGEX: str = "regex";    len = 5; break;
    case TYPE_FILE:  str = "file";     len = 4; break;
    case TYPE_RANGE: str = "range";    len = 5; break;
    case TYPE_TAB:   str = "table";    len = 5; break;
    case TYPE_RFN:
    case TYPE_CFN:   str = "function"; len = 8; break;
    default: break;
    }
    set_str(fp-1, riff_str_new(str, len));
    return 1;
}

// System functions

// clock()
LIB_FN(clock) {
    set_flt(fp-1, ((riff_float) clock() / (riff_float) CLOCKS_PER_SEC));
    return 1;
}

// exit([n])
LIB_FN(exit) {
    exit(argc ? intval(fp) : 0);
}

// Registry info for a given library function
#define LIB_FN_REG(name, arity) { #name , { (arity), l_##name } }

static struct {
    const char *name;
    riff_cfn    fn;
} lib_fn[] = {
    // Arithmetic
    LIB_FN_REG(abs,    1),
    LIB_FN_REG(atan,   1),
    LIB_FN_REG(ceil,   1),
    LIB_FN_REG(cos,    1),
    LIB_FN_REG(exp,    1),
    LIB_FN_REG(int,    1),
    LIB_FN_REG(log,    1),
    LIB_FN_REG(sin,    1),
    LIB_FN_REG(sqrt,   1),
    LIB_FN_REG(tan,    1),
    // I/O
    LIB_FN_REG(close,  1),
    LIB_FN_REG(eof,    0),
    LIB_FN_REG(eval,   1),
    LIB_FN_REG(flush,  0),
    LIB_FN_REG(get,    0),
    LIB_FN_REG(getc,   0),
    LIB_FN_REG(open,   1),
    LIB_FN_REG(print,  0),
    LIB_FN_REG(printf, 1),
    LIB_FN_REG(putc,   0),
    LIB_FN_REG(read,   0),
    LIB_FN_REG(write,  0),
    // PRNG
    LIB_FN_REG(rand,   0),
    LIB_FN_REG(srand,  0),
    // Strings
    LIB_FN_REG(byte,   1),
    LIB_FN_REG(char,   0),
    LIB_FN_REG(fmt,    1),
    LIB_FN_REG(gsub,   2),
    LIB_FN_REG(hex,    1),
    LIB_FN_REG(lower,  1),
    LIB_FN_REG(num,    1),
    LIB_FN_REG(split,  1),
    LIB_FN_REG(sub,    2),
    LIB_FN_REG(type,   1),
    LIB_FN_REG(upper,  1),
    // System
    LIB_FN_REG(clock,  0),
    LIB_FN_REG(exit,   0),
};

static void register_funcs(riff_htab *g) {
    // Initialize the PRNG with the current time
    riff_prng_seed(&prngs, time(0));
    FOREACH(lib_fn, i) {
        riff_htab_insert_cstr(g, lib_fn[i].name, &(riff_val) {TYPE_CFN, .cfn = &lib_fn[i].fn});
    }
}

// NOTE: Standard streams can't be cleanly declared in a static struct like the
// lib functions since the names (e.g. stdin) aren't compile-time constants
#define REGISTER_LIB_STREAM(name) \
    riff_file *name##_fh = malloc(sizeof(riff_file)); \
    *name##_fh = (riff_file) {name, FH_STD}; \
    riff_htab_insert_cstr(g, #name, &(riff_val){TYPE_FILE, .fh = name##_fh});

static void register_streams(riff_htab *g) {
    REGISTER_LIB_STREAM(stdin);
    REGISTER_LIB_STREAM(stdout);
    REGISTER_LIB_STREAM(stderr);
}

void l_register_builtins(riff_htab *g) {
    register_funcs(g);
    register_streams(g);
}
