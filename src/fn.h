#ifndef FN_H
#define FN_H

#include "code.h"
#include "types.h"

struct rf_fn {
    rf_str  *name;
    uint8_t  arity;
    rf_code *code;
};

void f_init(rf_fn *, rf_str *);

#endif
