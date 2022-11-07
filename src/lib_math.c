#include "lib.h"

#include <math.h>

// Basic arithmetic functions
// Strict arity of 1 where argument is uncondtionally coerced.
LIB_FN(ceil) { set_int(fp-1, (riff_int) ceil(fltval(fp))); return 1; }
LIB_FN(cos)  { set_flt(fp-1, cos(fltval(fp)));             return 1; }
LIB_FN(exp)  { set_flt(fp-1, exp(fltval(fp)));             return 1; }
LIB_FN(int)  { set_int(fp-1, intval(fp));                  return 1; }
LIB_FN(sin)  { set_flt(fp-1, sin(fltval(fp)));             return 1; }
LIB_FN(sqrt) { set_flt(fp-1, sqrt(fltval(fp)));            return 1; }
LIB_FN(tan)  { set_flt(fp-1, tan(fltval(fp)));             return 1; }

// abs(x)
LIB_FN(abs) {
    if (is_int(fp)) {
        set_int(fp-1, llabs(fp->i));
    } else {
        set_flt(fp-1, fabs(fltval(fp)));
    }
    return 1;
}

// atan(y[,x])
LIB_FN(atan) {
    if (argc == 1) {
        set_flt(fp-1, atan(fltval(fp)));
    } else if (argc > 1) {
        set_flt(fp-1, atan2(fltval(fp), fltval(fp+1)));
    }
    return 1;
}

// log(x[,b])
LIB_FN(log) {
    if (argc == 1) {
        set_flt(fp-1, log(fltval(fp)));
    } else if (fltval(fp+1) == 2.0) {
        set_flt(fp-1, log2(fltval(fp)));
    } else if (fltval(fp+1) == 10.0) {
        set_flt(fp-1, log10(fltval(fp)));
    } else {
        set_flt(fp-1, log(fltval(fp)) / log(fltval(fp+1)));
    }
    return 1;
}

static riff_lib_fn_reg mathlib[] = {
    LIB_FN_REG(abs,  1),
    LIB_FN_REG(atan, 1),
    LIB_FN_REG(ceil, 1),
    LIB_FN_REG(cos,  1),
    LIB_FN_REG(exp,  1),
    LIB_FN_REG(int,  1),
    LIB_FN_REG(log,  1),
    LIB_FN_REG(sin,  1),
    LIB_FN_REG(sqrt, 1),
    LIB_FN_REG(tan,  1),
};

void riff_lib_register_math(riff_htab *g) {
    FOREACH(mathlib, i) {
        riff_htab_insert_cstr(g, mathlib[i].name, &(riff_val) {TYPE_CFN, .cfn = &mathlib[i].fn});
    }
}
