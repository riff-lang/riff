#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "array.h"
#include "fn.h"
#include "mem.h"
#include "lib.h"

// Arithmetic functions

// abs(x)
static int l_abs(rf_val *fp, int argc) {
    if (is_int(fp))
        assign_int(fp-1, llabs(fp->u.i));
    else
        assign_flt(fp-1, fabs(fltval(fp)));
    return 1;
}

// atan(y[,x])
static int l_atan(rf_val *fp, int argc) {
    if (argc == 1)
        assign_flt(fp-1, atan(fltval(fp)));
    else if (argc > 1)
        assign_flt(fp-1, atan2(fltval(fp), fltval(fp+1)));
    return 1;
}

// ceil(x)
static int l_ceil(rf_val *fp, int argc) {
    assign_int(fp-1, (rf_int) ceil(fltval(fp)));
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

// log(x[,b])
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

// Pseudo-random number generation

// rand([x])
// Uses POSIX drand48() to produce random floats and two calls to
// lrand48()/mrand48() concatenated together to produce random 64-bit
// integers.
// Ex:
//   rand()  -> float in the range [0..1)
//   rand(0) -> random Riff integer (64 bit signed)
//   rand(n) -> random integer in the range [0..n]; n can be negative
// TODO handle rand(x,y), returning a random integer in the range of
// [x,y]
static int l_rand(rf_val *fp, int argc) {
    if (!argc) {
        assign_flt(fp-1, drand48());
    } else { 
        if (intval(fp) == 0) {
            int64_t n1 = mrand48();
            int64_t n2 = mrand48();
            rf_int n = (n1 << 32) ^ n2;
            assign_int(fp-1, n);
        } else {
            uint64_t n1 = lrand48();
            uint64_t n2 = lrand48();
            rf_int n = (n1 << 32) ^ n2;
            if (intval(fp) > 0)
                assign_int(fp-1, n % (intval(fp) + 1));
            else
                assign_int(fp-1, -(n % -(intval(fp) - 1)));
        }
    }
    return 1;
}

// srand([x])
// Initializes the PRNG with seed `x` or time(0) if no argument given.
// rand() will produce the same sequence when srand is initialized
// with a given seed every time.
// Ex:
//   srand(99); rand(); rand() -> 0.380257, 0.504358
static int l_srand(rf_val *fp, int argc) {
    if (!argc)
        srand48(time(0));
    else
        srand48((long) intval(fp));
    return 0;
}

// String functions

// byte(s[,i])
// Takes one string and an optional index argument `i` and returns the
// numeric ASCII code associated with that character. The default
// value for `i` is 0.
// If a user-defined function is passed, the byte at index `i` in the
// function's bytecode array is returned. This is the same as
// subscripting the function, i.e. byte(f,0) == f[0] for function f.
static int l_byte(rf_val *fp, int argc) {
    int idx = argc > 1 ? intval(fp+1) : 0;
    if (is_str(fp)) {
        if (idx > fp->u.s->l)
            idx = fp->u.s->l;
        assign_int(fp-1, fp->u.s->str[idx]);
    } else if (is_rfn(fp)) {
        if (idx > fp->u.fn->code->n)
            idx = fp->u.fn->code->n;
        assign_int(fp-1, fp->u.fn->code->code[idx]);
    } else {
        return 0;
    }
    return 1;
}

// char(...)
// Takes zero or more integers and returns a string composed of the
// ASCII codes of each respective argument in order
// Ex:
//   char(114, 105, 102, 102) -> "riff"
static int l_char(rf_val *fp, int argc) {
    if (!argc) return 0;
    char buf[argc + 1];
    for (int i = 0; i < argc; ++i) {
        buf[i] = intval(fp+i);
    }
    buf[argc] = '\0';
    assign_str(fp-1, s_newstr(buf, argc, 0));
    return 1;
}

// hex(x)
// Returns a string of `x` as an integer in hexadecimal (lowercase)
// with the leading "0x"
// Ex:
//   hex(255) -> "0xff"
static int l_hex(rf_val *fp, int argc) {
    rf_int i = intval(fp);
    size_t len = snprintf(NULL, 0, "0x%llx", i);
    char buf[len + 1];
    snprintf(buf, len + 1, "0x%llx", i);
    assign_str(fp-1, s_newstr(buf, len, 0));
    return 1;
}

static int allxcase(rf_val *fp, int c) {
    size_t len = fp->u.s->l;
    char str[len + 1];
    for (int i = 0; i < len; ++i) {
        str[i] = c ? toupper(fp->u.s->str[i])
                   : tolower(fp->u.s->str[i]);
    }
    str[len] = '\0';
    assign_str(fp-1, s_newstr(str, len, 0));
    return 1;
}

// lower(s)
static int l_lower(rf_val *fp, int argc) {
    if (!is_str(fp))
        return 0;
    return allxcase(fp, 0);
}

// split(s[,d])
// Returns an array with elements being string `s` split on delimiter
// `d`. The delimiter can be zero or more characters. If no delimiter
// is provided, the default is " ". If the delimiter is the empty
// string (""), the string is split into an array of individual
// characters.
static int l_split(rf_val *fp, int argc) {
    if (!is_str(fp))
        return 0;
    size_t len = fp->u.s->l + 1;
    char str[len];
    memcpy(str, fp->u.s->str, len);
    const char *delim = (argc < 2 || !is_str(fp+1)) ? " \t" : fp[1].u.s->str;
    rf_val *arr = v_newarr();
    rf_str *s;
    rf_val v;
    if (!strlen(delim)) {
        for (rf_int i = 0; i < len - 1; ++i) {
            s = s_newstr(str + i, 1, 0);
            v = (rf_val) {TYPE_STR, .u.s = s};
            a_insert_int(arr->u.a, i, &v, 1, 1);
        }
    } else {
        char *tk = strtok(str, delim);
        for (rf_int i = 0; tk; ++i) {
            s = s_newstr(tk, strlen(tk), 0);
            v = (rf_val) {TYPE_STR, .u.s = s};
            a_insert_int(arr->u.a, i, &v, 1, 1);
            tk = strtok(NULL, delim);
        }
    }
    fp[-1] = *arr;
    return 1;
}

// upper(s)
static int l_upper(rf_val *fp, int argc) {
    if (!is_str(fp))
        return 0;
    return allxcase(fp, 1);
}

static struct {
    const char *name;
    c_fn        fn;
} lib_fn[] = {
    // Arithmetic
    { "abs",   { 1, l_abs }   },
    { "atan",  { 1, l_atan }  },
    { "ceil",  { 1, l_ceil }  },
    { "cos",   { 1, l_cos }   },
    { "exp",   { 1, l_exp }   },
    { "int",   { 1, l_int }   },
    { "log",   { 1, l_log }   },
    { "sin",   { 1, l_sin }   },
    { "sqrt",  { 1, l_sqrt }  },
    { "tan",   { 1, l_tan }   },
    // PRNG
    { "rand",  { 0, l_rand }  },
    { "srand", { 0, l_srand } },
    // Strings
    { "byte",  { 1, l_byte }  },
    { "char",  { 0, l_char }  },
    { "hex",   { 1, l_hex }   },
    { "lower", { 1, l_lower } },
    { "split", { 1, l_split } },
    { "upper", { 1, l_upper } },
    { NULL,    { 0, NULL }    }
};

void l_register(hash_t *g) {
    // Initialize the PRNG with the current time
    srand48(time(0));
    for (int i = 0; lib_fn[i].name; ++i) {
        rf_str *s  = s_newstr(lib_fn[i].name, strlen(lib_fn[i].name), 1);
        rf_val *fn = malloc(sizeof(rf_val));
        *fn        = (rf_val) {TYPE_CFN, .u.cfn = &lib_fn[i].fn};
        h_insert(g, s, fn, 1);
    }
}
