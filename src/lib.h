#ifndef LIB_H
#define LIB_H

#include "hash.h"
#include "types.h"

// Each library function takes a frame pointer and an argument count,
// which allows the functions to be variadic.
//
// The FP will always point to the first argument. FP+1 will hold the
// second argument, etc. The VM expects the return value (if
// applicable) at FP-1.
typedef int (* rf_lib_fn) (rf_val *, int);

struct c_fn {
    // Minimum arity for the function. The VM currently compensates
    // for an insufficient amount of arguments by nullifying stack
    // elements before passing the FP to the function.
    uint8_t   arity;
    rf_lib_fn fn;
};

void l_register(rf_htab *);

#endif
