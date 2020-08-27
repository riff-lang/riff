#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdlib.h>

#define TYPE_NULL 1
#define TYPE_INT  2
#define TYPE_FLT  4
#define TYPE_STR  8
#define TYPE_ARR  16
#define TYPE_RFN  32
#define TYPE_CFN  64

#define is_null(x) ((x)->type & TYPE_NULL)
#define is_int(x)  ((x)->type & TYPE_INT)
#define is_flt(x)  ((x)->type & TYPE_FLT)
#define is_str(x)  ((x)->type & TYPE_STR)
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

typedef struct rf_arr rf_arr;
typedef struct rf_fn  rf_fn;
typedef struct c_fn   c_fn;

typedef struct {
    // Type tag is aligned to the 64-bit boundary to accommodate an
    // implicit type tag in the VM stack element union typedef.
    uint64_t type;
    union {
        rf_flt  f;
        rf_int  i;
        rf_str *s;
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
    assign_int(l, (intval(l) op intval(r)));

#define flt_arith(l,r,op) \
    assign_flt(l, (numval(l) op numval(r)));

#define num_arith(l,r,op) \
    if (is_flt(l) || is_flt(r)) { \
        flt_arith(l,r,op); \
    } else { \
        int_arith(l,r,op); \
    }

rf_int    str2int(rf_str *);
rf_flt    str2flt(rf_str *);

uint32_t  s_hash(const char *);
rf_str    *s_newstr(const char *, size_t, int);
rf_str    *s_concat(rf_str *, rf_str *, int);
rf_str    *s_int2str(rf_int);
rf_str    *s_flt2str(rf_flt);
rf_val    *v_newnull(void);
rf_val    *v_newint(rf_int);
rf_val    *v_newflt(rf_flt);
rf_val    *v_newstr(rf_str *);
rf_val    *v_newarr(void);

#endif
