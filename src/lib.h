#ifndef LIB_H
#define LIB_H

#include "table.h"
#include "value.h"

// Each library function takes a frame pointer and an argument count,
// which allows the functions to be variadic.
//
// The FP will always point to the first argument. FP+1 will hold the
// second argument, etc. The VM expects the return value (if
// applicable) at FP-1.
typedef int (* riff_lib_fn) (riff_val *, int);

struct riff_cfn {
    // Minimum arity for the function. The VM currently compensates
    // for an insufficient amount of arguments by nullifying stack
    // elements before passing the FP to the function.
    uint8_t     arity;
    riff_lib_fn fn;
};

void l_register_builtins(riff_htab *);

#endif
