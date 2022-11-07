#ifndef LIB_H
#define LIB_H

#include "string.h"
#include "table.h"
#include "value.h"

// Each library function takes a frame pointer and an argument count, which
// allows the functions to be variadic.
//
// The FP will always point to the first argument. FP+1 will hold the second
// argument, etc. The VM expects the return value (if applicable) at FP-1.
typedef int (* riff_lib_fn) (riff_val *, int);

// Convenience macro for library function signatures
#define LIB_FN(name) static int l_##name(riff_val *fp, int argc)

struct riff_cfn {
    // Minimum arity for the function. The VM currently compensates for an
    // insufficient amount of arguments by nullifying stack elements before
    // passing the FP to the function.
    uint8_t     arity;
    riff_lib_fn fn;
};

// Registry info
typedef struct {
    const char *name;
    riff_cfn    fn;
} riff_lib_fn_reg;

#define LIB_FN_REG(name, arity) { #name , { (arity), l_##name } }

static inline int build_char_str(riff_val *fp, int argc, char *buf) {
    int n = 0;
    for (int i = 0; i < argc; ++i) {
        uint32_t c = (uint32_t) intval(fp+i);
        if (c <= 0x7f) {
            buf[n++] = (char) c;
        } else {
            char ubuf[8];
            int j = 8 - riff_unicodetoutf8(ubuf, c);
            for (; j < 8; ++j) {
                buf[n++] = ubuf[j];
            }
        }
    }
    return n;
}

static inline void fputs_val(FILE *f, riff_val *v) {
    char buf[STR_BUF_SZ];
    char *p = buf;
    riff_tostr(v, &p);
    fputs(p, f);
}

void riff_lib_register_base(riff_htab *);
void riff_lib_register_io(riff_htab *);
void riff_lib_register_math(riff_htab *);
void riff_lib_register_os(riff_htab *);
void riff_lib_register_prng(riff_htab *);
void riff_lib_register_str(riff_htab *);

#endif
