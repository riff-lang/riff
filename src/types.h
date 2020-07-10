#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdlib.h>

#include "str.h"

#define TYPE_INT 1  // 000001
#define TYPE_FLT 2  // 000010
#define TYPE_STR 4  // 000100
#define TYPE_ARR 8  // 001000
#define TYPE_FN  16 // 010000
#define TYPE_SYM 32 // 100000

#define IS_FLT(x) (x.type & TYPE_FLT)
#define IS_INT(x) (x.type & TYPE_INT)
#define IS_STR(x) (x.type & TYPE_STR)
#define IS_ARR(x) (x.type & TYPE_ARR)
#define IS_FN(x)  (x.type & TYPE_FN)
#define IS_SYM(x) (x.type & TYPE_SYM)

#define IS_NUM(x) (x.type & (TYPE_INT | TYPE_FLT))

typedef double  flt_t;
typedef int64_t int_t;

#endif
