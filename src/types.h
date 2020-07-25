#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdlib.h>

#define TYPE_VOID 1  // 000001
#define TYPE_INT  2  // 000010
#define TYPE_FLT  4  // 000100
#define TYPE_STR  8  // 001000
#define TYPE_ARR  16 // 010000
#define TYPE_FN   32 // 100000

#define IS_VOID(x) (x->type & TYPE_VOID)
#define IS_INT(x)  (x->type & TYPE_INT)
#define IS_FLT(x)  (x->type & TYPE_FLT)
#define IS_STR(x)  (x->type & TYPE_STR)
#define IS_ARR(x)  (x->type & TYPE_ARR)
#define IS_FN(x)   (x->type & TYPE_FN)

#define IS_NUM(x)  (x->type & (TYPE_INT | TYPE_FLT))

typedef double  flt_t;
typedef int64_t int_t;

typedef struct {
    size_t    l;
    uint32_t  hash;
    char     *str;
} str_t;

// Forward declaration
typedef struct arr_t arr_t;

typedef struct {
    int type;
    union {
        flt_t  f;
        int_t  i;
        str_t *s;
        arr_t *a;
        // Add function
    } u;
} val_t;

// Assign value x to val_t *p
#define assign_int(p, x) (p)->u.i = (x); (p)->type = TYPE_INT
#define assign_flt(p, x) (p)->u.f = (x); (p)->type = TYPE_FLT
#define assign_str(p, x) (p)->u.s = (x); (p)->type = TYPE_STR

uint32_t  s_hash(const char *);
str_t    *s_newstr(const char *, size_t, int);
str_t    *s_concat(str_t *, str_t *, int);
str_t    *s_int2str(int_t);
str_t    *s_flt2str(flt_t);
val_t    *v_newvoid(void);
val_t    *v_newint(int_t);
val_t    *v_newflt(flt_t);
val_t    *v_newstr(str_t *);
val_t    *v_newarr(void);

#endif
