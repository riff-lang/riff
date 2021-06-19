#ifndef TYPES_H
#define TYPES_H

// Must precede pcre2.h inclusion
#define PCRE2_CODE_UNIT_WIDTH 8

#include <pcre2.h>
#include <stdint.h>
#include <stdlib.h>

#include "util.h"

enum types {
    TYPE_NULL,
    TYPE_INT,
    TYPE_FLT,
    TYPE_STR,
    TYPE_RE,
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
#define is_seq(x)  ((x)->type == TYPE_SEQ)
#define is_tbl(x)  ((x)->type == TYPE_TBL)
#define is_rfn(x)  ((x)->type == TYPE_RFN)
#define is_cfn(x)  ((x)->type == TYPE_CFN)
#define is_num(x)  ((x)->type == TYPE_INT || (x)->type == TYPE_FLT)
#define is_fn(x)   ((x)->type == TYPE_RFN || (x)->type == TYPE_CFN)

typedef double  rf_flt;
typedef int64_t rf_int;

typedef struct {
    size_t    l;
    uint32_t  hash;
    char     *str;
} rf_str;

typedef pcre2_code rf_re;

#define RE_ANCHORED       PCRE2_ANCHORED
#define RE_ICASE          PCRE2_CASELESS
#define RE_DOLLAREND      PCRE2_DOLLAR_ENDONLY
#define RE_DOTALL         PCRE2_DOTALL
#define RE_DUPNAMES       PCRE2_DUPNAMES
#define RE_EXTENDED       PCRE2_EXTENDED
#define RE_IGNORE_BAD_ESC PCRE2_EXTRA_BAD_ESCAPE_IS_LITERAL
#define RE_LITERAL        PCRE2_LITERAL
#define RE_MULTILINE      PCRE2_MULTILINE
#define RE_UNGREEDY       PCRE2_UNGREEDY


// Default compile options for regular expressions
#define RE_CFLAGS       RE_EXTENDED | RE_DUPNAMES
#define RE_CFLAGS_EXTRA RE_IGNORE_BAD_ESC

typedef struct {
    rf_int from;
    rf_int to;
    rf_int itvl;
} rf_seq;

typedef struct rf_tbl rf_tbl;
typedef struct rf_fn  rf_fn;
typedef struct c_fn   c_fn;

typedef struct {
    // Type tag is aligned to the 64-bit boundary to accommodate an
    // implicit type tag in the VM stack element. This is necessary
    // for distinguishing values from addresses on the VM stack for
    // instructions which operate on both. E.g. array subscripting.
    uint64_t type;
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
#define assign_null(p)   (p)->type = TYPE_NULL
#define assign_int(p, x) *(p) = (rf_val) {TYPE_INT, .u.i = (x)}
#define assign_flt(p, x) *(p) = (rf_val) {TYPE_FLT, .u.f = (x)}
#define assign_str(p, x) *(p) = (rf_val) {TYPE_STR, .u.s = (x)}

#define numval(x) (is_int(x) ? (x)->u.i : \
                   is_flt(x) ? (x)->u.f : \
                   is_str(x) ? str2flt((x)->u.s) : 0)
#define intval(x) (is_int(x) ? (x)->u.i : \
                   is_flt(x) ? (rf_int) (x)->u.f : \
                   is_str(x) ? str2int((x)->u.s) : 0)
#define fltval(x) (is_flt(x) ? (x)->u.f : \
                   is_int(x) ? (rf_flt) (x)->u.i : \
                   is_str(x) ? str2flt((x)->u.s) : 0)

// == and != operators
#define cmp_eq(l,r,op) \
    if (is_null(l) ^ is_null(r)) { \
        assign_int(l, !(0 op 0)); \
    } else if (is_str(l) && is_str(r)) { \
        if (!l->u.s->hash) l->u.s->hash = u_strhash(l->u.s->str); \
        if (!r->u.s->hash) r->u.s->hash = u_strhash(r->u.s->str); \
        assign_int(l, (l->u.s->hash op r->u.s->hash)); \
    } else if (is_str(l) && !is_str(r)) { \
        if (!l->u.s->l) { \
            assign_int(l, !(0 op 0)); \
            return; \
        } \
        char *end; \
        rf_flt f = u_str2d(l->u.s->str, &end, 0); \
        if (*end != '\0') { \
            assign_int(l, 0); \
        } else { \
            assign_int(l, (f op numval(r))); \
        } \
    } else if (!is_str(l) && is_str(r)) { \
        if (!r->u.s->l) { \
            assign_int(l, !(0 op 0)); \
            return; \
        } \
        char *end; \
        rf_flt f = u_str2d(r->u.s->str, &end, 0); \
        if (*end != '\0') { \
            assign_int(l, 0); \
        } else { \
            assign_int(l, (numval(l) op f)); \
        } \
    } else { \
        num_arith(l,r,op); \
    }

// >, <, >= and <= operators
#define cmp_rel(l,r,op) num_arith(l,r,op);

rf_int  str2int(rf_str *);
rf_flt  str2flt(rf_str *);
rf_re  *re_compile(char *, int, int *);
void    re_free(rf_re *);
rf_int  re_match(char *, rf_re *);
int     re_sub(char *, rf_re *, char *, char *, size_t *, int);
rf_str *s_newstr(const char *, size_t, int);
rf_str *s_substr(rf_str *, rf_int, rf_int, rf_int);
rf_str *s_concat(rf_str *, rf_str *, int);
rf_str *s_int2str(rf_int);
rf_str *s_flt2str(rf_flt);
rf_val *v_newnull(void);
rf_val *v_newint(rf_int);
rf_val *v_newflt(rf_flt);
rf_val *v_newstr(rf_str *);
rf_val *v_newtbl(void);

#endif
