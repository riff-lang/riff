#ifndef TYPES_H
#define TYPES_H

#include "util.h"

// Must precede pcre2.h inclusion
#define PCRE2_CODE_UNIT_WIDTH 8

#include <pcre2.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum types {
    TYPE_NULL,
    TYPE_INT,
    TYPE_FLT,
    TYPE_STR,
    TYPE_RE,
    TYPE_FH,
    TYPE_SEQ,
    TYPE_TBL,
    TYPE_RFN,
    TYPE_CFN
};

#define is_null(x) (!(x)->type)
#define is_int(x)  ((x)->type == TYPE_INT)
#define is_flt(x)  ((x)->type == TYPE_FLT)
#define is_str(x)  ((x)->type == TYPE_STR)
#define is_re(x)   ((x)->type == TYPE_RE)
#define is_fh(x)   ((x)->type == TYPE_FH)
#define is_seq(x)  ((x)->type == TYPE_SEQ)
#define is_tbl(x)  ((x)->type == TYPE_TBL)
#define is_rfn(x)  ((x)->type == TYPE_RFN)
#define is_cfn(x)  ((x)->type == TYPE_CFN)
#define is_num(x)  ((x)->type == TYPE_INT || (x)->type == TYPE_FLT)
#define is_fn(x)   ((x)->type == TYPE_RFN || (x)->type == TYPE_CFN)

typedef int64_t  rf_int;
typedef uint64_t rf_uint;
typedef double   rf_flt;

typedef struct {
    uint32_t  hash;
    size_t    l;
    char     *str;
} rf_str;

#define TEMP_HASHED_STR(s,h,l) &(rf_str){(h), (l), (s)}
#define TEMP_STR(s,l)          &(rf_str){0,   (l), (s)}

typedef pcre2_code rf_re;

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
// Can only be set via pcre2_set_compile_extra_options() before
// calling pcre2_compile()
#define RE_IGNORE_BAD_ESC  PCRE2_EXTRA_BAD_ESCAPE_IS_LITERAL

// Default compile options for regular expressions
#define RE_CFLAGS          RE_DUPNAMES
#define RE_CFLAGS_EXTRA    RE_IGNORE_BAD_ESC

typedef struct {
    FILE     *p;
    uint32_t  flags;
} rf_fh;

typedef struct {
    rf_int from;
    rf_int to;
    rf_int itvl;
} rf_seq;

typedef struct rf_tbl rf_tbl;
typedef struct rf_fn  rf_fn;
typedef struct c_fn   c_fn;

typedef struct {
    // Type tag is aligned to the word-sized boundary to accommodate
    // an implicit type tag in the VM stack element. This is necessary
    // for distinguishing values from addresses on the VM stack for
    // instructions which operate on both. E.g. array subscripting.
    uintptr_t type;
    union {
        rf_flt  f;
        rf_int  i;
        rf_str *s;
        rf_re  *r;
        rf_seq *q;
        rf_tbl *t;
        rf_fn  *fn;
        c_fn   *cfn;
    } u;
} rf_val;

// Assign value x to rf_val *p
#define set_null(p)   (p)->type = TYPE_NULL
#define set_int(p, x) *(p) = (rf_val) {TYPE_INT, .u.i = (x)}
#define set_flt(p, x) *(p) = (rf_val) {TYPE_FLT, .u.f = (x)}
#define set_str(p, x) *(p) = (rf_val) {TYPE_STR, .u.s = (x)}

#define numval(x) (is_int(x) ? (x)->u.i : \
                   is_flt(x) ? (x)->u.f : \
                   is_str(x) ? str2flt((x)->u.s) : 0)
#define intval(x) (is_int(x) ? (x)->u.i : \
                   is_flt(x) ? (rf_int) (x)->u.f : \
                   is_str(x) ? str2int((x)->u.s) : 0)
#define fltval(x) (is_flt(x) ? (x)->u.f : \
                   is_int(x) ? (rf_flt) (x)->u.i : \
                   is_str(x) ? str2flt((x)->u.s) : 0)

rf_int  str2int(rf_str *);
rf_flt  str2flt(rf_str *);
rf_str *s_newstr(const char *, size_t, int);
rf_str *s_newstr_concat(char *, char *, int);
rf_str *s_substr(char *, rf_int, rf_int, rf_int);
rf_str *s_int2str(rf_int);
rf_str *s_flt2str(rf_flt);
void    re_register_fldv(rf_tbl *);
rf_re  *re_compile(char *, uint32_t, int *);
void    re_free(rf_re *);
int     re_store_numbered_captures(pcre2_match_data *);
rf_int  re_match(char *, rf_re *, int);
rf_fh  *io_fopen(char *, char *);
rf_val *v_newnull(void);
rf_val *v_newint(rf_int);
rf_val *v_newflt(rf_flt);
rf_val *v_newstr(rf_str *);
rf_val *v_newtbl(void);

#endif
