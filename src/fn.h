#ifndef FN_H
#define FN_H

#include "code.h"
#include "types.h"

struct rf_fn {
    rf_code *code;
    rf_str  *name;
    uint8_t  arity;
};

void f_init(rf_fn *, rf_str *);

#endif
