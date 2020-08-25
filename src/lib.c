#include <math.h>
#include <string.h>

#include "lib.h"

// atan2(y,x)
static int l_atan2(rf_val *sp, rf_val *fp) {
    assign_flt(sp, atan2(fltval(fp), fltval(fp+1)));
    return 1;
}

// cos(x)
static int l_cos(rf_val *sp, rf_val *fp) {
    assign_flt(sp, cos(fltval(fp)));
    return 1;
}

// exp(x)
static int l_exp(rf_val *sp, rf_val *fp) {
    assign_flt(sp, exp(fltval(fp)));
    return 1;
}

// sin(x)
static int l_sin(rf_val *sp, rf_val *fp) {
    assign_flt(sp, sin(fltval(fp)));
    return 1;
}

// sqrt(x)
static int l_sqrt(rf_val *sp, rf_val *fp) {
    assign_flt(sp, sqrt(fltval(fp)));
    return 1;
}

static struct {
    const char *name;
    c_fn        fn;
} lib_fn[] = {
    { "atan2", {2, l_atan2} },
    { "cos",   {1, l_cos}   },
    { "exp",   {1, l_exp}   },
    { "sin",   {1, l_sin}   },
    { "sqrt",  {1, l_sqrt}  },
    // TODO hack for l_register loop condition
    { NULL,    {0, NULL}    }
};

void l_register(hash_t *g) {
    for (int i = 0; lib_fn[i].name; ++i) {
        rf_str *s  = s_newstr(lib_fn[i].name, strlen(lib_fn[i].name), 1);
        rf_val *fn = malloc(sizeof(rf_val));
        *fn        = (rf_val) {TYPE_CFN, .u.cfn = &lib_fn[i].fn};
        h_insert(g, s, fn, 1);
    }
}
