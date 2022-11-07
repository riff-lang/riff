#include "lib.h"

#include <time.h>

// System functions

// clock()
LIB_FN(clock) {
    set_flt(fp-1, ((riff_float) clock() / (riff_float) CLOCKS_PER_SEC));
    return 1;
}

// exit([n])
LIB_FN(exit) {
    exit(argc ? intval(fp) : 0);
}

static riff_lib_fn_reg oslib[] = {
    LIB_FN_REG(clock, 0),
    LIB_FN_REG(exit,  0),
};

void riff_lib_register_os(riff_htab *g) {
    FOREACH(oslib, i) {
        riff_htab_insert_cstr(g, oslib[i].name, &(riff_val) {TYPE_CFN, .cfn = &oslib[i].fn});
    }
}
