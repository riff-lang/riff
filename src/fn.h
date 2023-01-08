#ifndef FN_H
#define FN_H

#include "code.h"
#include "value.h"

struct riff_fn {
    riff_code  code;
    uint8_t    arity;
    riff_str  *name;
};

void riff_fn_init(riff_fn *);

#endif
