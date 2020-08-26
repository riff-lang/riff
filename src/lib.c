#include <math.h>
#include <string.h>

#include "lib.h"

// Arithmetic functions

// atan(y [,x])
static int l_atan(rf_val *fp, int argc) {
    if (argc == 1)
        assign_flt(fp-1, atan(fltval(fp)));
    else if (argc > 1)
        assign_flt(fp-1, atan2(fltval(fp), fltval(fp+1)));
    return 1;
}

// cos(x)
static int l_cos(rf_val *fp, int argc) {
    assign_flt(fp-1, cos(fltval(fp)));
    return 1;
}

// exp(x)
static int l_exp(rf_val *fp, int argc) {
    assign_flt(fp-1, exp(fltval(fp)));
    return 1;
}

// int(x)
static int l_int(rf_val *fp, int argc) {
    assign_int(fp-1, intval(fp));
    return 1;
}

// log(x [,b])
static int l_log(rf_val *fp, int argc) {
    if (argc == 1)
        assign_flt(fp-1, log(fltval(fp)));
    else if (fltval(fp+1) == 2.0)
        assign_flt(fp-1, log2(fltval(fp)));
    else if (fltval(fp+1) == 10.0)
        assign_flt(fp-1, log10(fltval(fp)));
    else
        assign_flt(fp-1, log(fltval(fp)) / log(fltval(fp+1)));
    return 1;
}

// sin(x)
static int l_sin(rf_val *fp, int argc) {
    assign_flt(fp-1, sin(fltval(fp)));
    return 1;
}

// sqrt(x)
static int l_sqrt(rf_val *fp, int argc) {
    assign_flt(fp-1, sqrt(fltval(fp)));
    return 1;
}

// tan(x)
static int l_tan(rf_val *fp, int argc) {
    assign_flt(fp-1, tan(fltval(fp)));
    return 1;
}

// String functions

// char(...)
// Takes zero or more integers and returns a string composed of the
// ASCII codes of each respective argument in order
// Example:
//   char(114, 105, 102, 102) -> "riff"
static int l_char(rf_val *fp, int argc) {
    char buf[argc + 1];
    for (int i = 0; i < argc; ++i) {
        buf[i] = intval(fp+i);
    }
    buf[argc] = '\0';
    assign_str(fp-1, s_newstr(buf, argc, 0));
    return 1;
}

static struct {
    const char *name;
    c_fn        fn;
} lib_fn[] = {
    { "atan", { 1, l_atan } },
    { "cos",  { 1, l_cos }  },
    { "exp",  { 1, l_exp }  },
    { "int",  { 1, l_int }  },
    { "log",  { 1, l_log }  },
    { "sin",  { 1, l_sin }  },
    { "sqrt", { 1, l_sqrt } },
    { "tan",  { 1, l_tan }  },
    { "char", { 0, l_char } },
    // TODO hack for l_register loop condition
    { NULL,   { 0, NULL }   }
};

void l_register(hash_t *g) {
    for (int i = 0; lib_fn[i].name; ++i) {
        rf_str *s  = s_newstr(lib_fn[i].name, strlen(lib_fn[i].name), 1);
        rf_val *fn = malloc(sizeof(rf_val));
        *fn        = (rf_val) {TYPE_CFN, .u.cfn = &lib_fn[i].fn};
        h_insert(g, s, fn, 1);
    }
}
