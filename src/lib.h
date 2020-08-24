#ifndef LIB_H
#define LIB_H

#include "hash.h"
#include "types.h"

typedef int (* rf_lib_fn) (rf_val *, rf_val *);

struct c_fn {
    uint8_t    arity;
    rf_lib_fn fn;
};

void l_register(hash_t *);

#endif
