#ifndef FN_H
#define FN_H

#include "code.h"
#include "types.h"

struct riff_fn {
    riff_code *code;
    riff_str  *name;
    uint8_t  arity;
};

void f_init(riff_fn *, riff_str *);

#endif
