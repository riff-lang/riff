#ifndef VALUE_H
#define VALUE_H

#include "util.h"

// Must precede pcre2.h inclusion
#define PCRE2_CODE_UNIT_WIDTH 8

#include <pcre2.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum types {
    TYPE_NULL,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STR,
    TYPE_REGEX,
    TYPE_FILE,
    TYPE_RANGE,
    TYPE_TAB,
    TYPE_RFN,
    TYPE_CFN
};

#define is_null(x)  (!(x)->type)
#define is_int(x)   ((x)->type == TYPE_INT)
#define is_float(x) ((x)->type == TYPE_FLOAT)
#define is_str(x)   ((x)->type == TYPE_STR)
#define is_regex(x) ((x)->type == TYPE_REGEX)
#define is_file(x)  ((x)->type == TYPE_FILE)
#define is_range(x) ((x)->type == TYPE_RANGE)
#define is_tab(x)   ((x)->type == TYPE_TAB)
#define is_rfn(x)   ((x)->type == TYPE_RFN)
#define is_cfn(x)   ((x)->type == TYPE_CFN)

#define is_num(x)   ((x)->type == TYPE_INT || (x)->type == TYPE_FLOAT)
#define is_fn(x)    ((x)->type == TYPE_RFN || (x)->type == TYPE_CFN)

typedef int64_t         riff_int;
typedef uint64_t        riff_uint;
typedef double          riff_float;
typedef uint32_t        strhash;
typedef struct riff_str riff_str;

struct riff_str {
    strhash   hash;
    uint8_t   hints;
    uint8_t   extra;
    size_t    len;
    char     *str;
    riff_str *next;
};

typedef pcre2_code riff_regex;

// Standard PCRE2 compile options
#define RE_ANCHORED        PCRE2_ANCHORED
#define RE_ICASE           PCRE2_CASELESS
#define RE_DOLLAREND       PCRE2_DOLLAR_ENDONLY
#define RE_DOTALL          PCRE2_DOTALL
#define RE_DUPNAMES        PCRE2_DUPNAMES
#define RE_EXTENDED        PCRE2_EXTENDED
#define RE_EXTENDED_MORE   PCRE2_EXTENDED_MORE
#define RE_LITERAL         PCRE2_LITERAL
#define RE_MULTILINE       PCRE2_MULTILINE
#define RE_NO_AUTO_CAPTURE PCRE2_NO_AUTO_CAPTURE
#define RE_UNGREEDY        PCRE2_UNGREEDY
#define RE_UNICODE         PCRE2_UCP

// Extra PCRE2 compile options
// Can only be set via pcre2_set_compile_extra_options() before calling
// pcre2_compile()
#define RE_IGNORE_BAD_ESC  PCRE2_EXTRA_BAD_ESCAPE_IS_LITERAL

// Default compile options for regular expressions
#define RE_CFLAGS          RE_DUPNAMES
#define RE_CFLAGS_EXTRA    RE_IGNORE_BAD_ESC

#define FH_STD 1

typedef struct {
    FILE     *p;
    uint32_t  flags;
} riff_file;

typedef struct {
    riff_int from;
    riff_int to;
    riff_int itvl;
} riff_range;

typedef struct riff_tab  riff_tab;
typedef struct riff_htab riff_htab;
typedef struct riff_fn   riff_fn;
typedef struct riff_cfn  riff_cfn;

typedef struct {
    uint8_t type;
    union {
        riff_int    i;
        riff_float  f;
        riff_str   *s;
        riff_regex *r;
        riff_file  *fh;
        riff_range *q;
        riff_tab   *t;
        riff_fn    *fn;
        riff_cfn   *cfn;
    };
} riff_val;

// Assign value x to riff_val *p
#define set_null(p)   (p)->type = TYPE_NULL
#define set_int(p, x) *(p) = (riff_val) {TYPE_INT,   .i = (x)}
#define set_flt(p, x) *(p) = (riff_val) {TYPE_FLOAT, .f = (x)}
#define set_str(p, x) *(p) = (riff_val) {TYPE_STR,   .s = (x)}
#define set_tab(p, x) *(p) = (riff_val) {TYPE_TAB,   .t = (x)}

#define numval(x) (is_int(x)   ? (x)->i : \
                   is_float(x) ? (x)->f : \
                   is_str(x)   ? str2flt((x)->s) : 0)
#define intval(x) (is_int(x)   ? (x)->i : \
                   is_float(x) ? (riff_int) (x)->f : \
                   is_str(x)   ? str2int((x)->s) : 0)
#define fltval(x) (is_float(x) ? (x)->f : \
                   is_int(x)   ? (riff_float) (x)->i : \
                   is_str(x)   ? str2flt((x)->s) : 0)

static inline riff_int str2int(riff_str *s) {
    char *end;
    return riff_strtoll(s->str, &end, 0);
}

static inline riff_float str2flt(riff_str *s) {
    char *end;
    return riff_strtod(s->str, &end, 0);
}

void        re_register_fldv(riff_tab *);
riff_regex *re_compile(char *, size_t, uint32_t, int *);
void        re_free(riff_regex *);
int         re_store_numbered_captures(pcre2_match_data *);
riff_int    re_match(char *, size_t, riff_regex *, int);
riff_val   *v_newnull(void);
riff_val   *v_newtab(uint32_t);
riff_val   *v_copy(riff_val *);
void        riff_tostr(riff_val *, char *);

#endif
