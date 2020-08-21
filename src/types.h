#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdlib.h>

#define TYPE_NULL 1  // 000001
#define TYPE_INT  2  // 000010
#define TYPE_FLT  4  // 000100
#define TYPE_STR  8  // 001000
#define TYPE_ARR  16 // 010000
#define TYPE_FN   32 // 100000

#define is_null(x) (x->type & TYPE_NULL)
#define is_int(x)  (x->type & TYPE_INT)
#define is_flt(x)  (x->type & TYPE_FLT)
#define is_str(x)  (x->type & TYPE_STR)
#define is_arr(x)  (x->type & TYPE_ARR)
#define is_fn(x)   (x->type & TYPE_FN)

#define is_num(x)  (x->type & (TYPE_INT | TYPE_FLT))

typedef double  rf_flt;
typedef int64_t rf_int;

typedef struct {
    size_t    l;
    uint32_t  hash;
    char     *str;
} rf_str;

typedef struct rf_arr rf_arr;
typedef struct rf_fn  rf_fn;

typedef struct {
    int type;
    union {
        rf_flt  f;
        rf_int  i;
        rf_str *s;
        rf_arr *a;
        rf_fn  *fn;
    } u;
} rf_val;

// Assign value x to rf_val *p
#define assign_null(p)   (p)->type = TYPE_NULL
#define assign_int(p, x) *p = (rf_val) {TYPE_INT, .u.i = (x)}
#define assign_flt(p, x) *p = (rf_val) {TYPE_FLT, .u.f = (x)}
#define assign_str(p, x) *p = (rf_val) {TYPE_STR, .u.s = (x)}

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
