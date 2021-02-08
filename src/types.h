#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdlib.h>

#include "util.h"

#define TYPE_NULL 1
#define TYPE_INT  2
#define TYPE_FLT  4
#define TYPE_STR  8
#define TYPE_SEQ  16
#define TYPE_ARR  32
#define TYPE_RFN  64
#define TYPE_CFN  128

#define is_null(x) ((x)->type & TYPE_NULL)
#define is_int(x)  ((x)->type & TYPE_INT)
#define is_flt(x)  ((x)->type & TYPE_FLT)
#define is_str(x)  ((x)->type & TYPE_STR)
#define is_seq(x)  ((x)->type & TYPE_SEQ)
#define is_arr(x)  ((x)->type & TYPE_ARR)
#define is_rfn(x)  ((x)->type & TYPE_RFN)
#define is_cfn(x)  ((x)->type & TYPE_CFN)
#define is_num(x)  ((x)->type & (TYPE_INT | TYPE_FLT))
#define is_fn(x)   ((x)->type & (TYPE_RFN | TYPE_CFN))

typedef double  rf_flt;
typedef int64_t rf_int;

typedef struct {
    size_t    l;
    uint32_t  hash;
    char     *str;
} rf_str;

typedef struct {
    rf_int from;
    rf_int to;
    rf_int itvl;
} rf_seq;

typedef struct rf_arr rf_arr;
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
        rf_seq *q;
        rf_arr *a;
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

#define int_arith(l,r,op) \
    if (is_int(l) && is_int(r)) { \
        l->u.i = (l->u.i op r->u.i); \
    } else { \
        assign_int(l, (intval(l) op intval(r))); \
    }

#define flt_arith(l,r,op) \
    assign_flt(l, (numval(l) op numval(r)));

#define num_arith(l,r,op) \
    if (is_int(l) && is_int(r)) { \
        l->u.i = (l->u.i op r->u.i); \
    } else { \
        flt_arith(l,r,op); \
    }

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
rf_str *s_newstr(const char *, size_t, int);
rf_str *s_substr(rf_str *, rf_int, rf_int, rf_int);
rf_str *s_concat(rf_str *, rf_str *, int);
rf_str *s_int2str(rf_int);
rf_str *s_flt2str(rf_flt);
rf_val *v_newnull(void);
rf_val *v_newint(rf_int);
rf_val *v_newflt(rf_flt);
rf_val *v_newstr(rf_str *);
rf_val *v_newarr(void);

#endif
