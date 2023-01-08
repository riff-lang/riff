#include "lib.h"

#include "parse.h"
#include "state.h"
#include "string.h"
#include "util.h"
#include "vm.h"

#include <ctype.h>
#include <errno.h>
#include <string.h>

static void err(const char *msg) {
    fprintf(stderr, "riff: %s\n", msg);
    exit(1);
}

#include "vm_ops.h"

LIB_FN(error);

// assert(e[,s])
LIB_FN(assert) {
    if (riff_unlikely(!argc)) {
        err("expected expression for assertion");
    }
    if (riff_unlikely(!vm_test(fp))) {
        l_error(fp+1, argc-1);
    }
    return 0;
}

// error([s])
LIB_FN(error) {
    if (argc && !is_null(fp)) {
        fputs_val(stderr, fp);
    }
    exit(1);
}

// eval(s)
LIB_FN(eval) {
    if (!is_str(fp))
        return 0;
    riff_state s;

    riff_state_init(&s);
    s.src = fp->s->str;
    riff_compile(&s);
    vm_exec_reenter(&s, (vm_stack *) fp);
    return 0;
}

// num(s[,b])
// Takes a string `s` and an optional base `b` and returns the interpreted num.
// The base can be 0 or an integer in the range [2..36]. The default base is 0.
// This function is more or less a direct interface to `strtoll()`.
LIB_FN(num) {
    if (!is_str(fp)) {
        if (is_int(fp)) {
            set_int(fp-1, fp->i);
        } else if (is_float(fp)) {
            set_flt(fp-1, fp->f);
        } else {
            set_int(fp-1, 0);
        }
        return 1;
    }
    int base = 0;
    if (argc > 1) {
        base = intval(fp+1);
    }
    char *end;
    errno = 0;
    riff_int i = riff_strtoll(fp->s->str, &end, base);
    if (errno == ERANGE || isdigit(*end)) {
        goto ret_flt;
    }
    if (*end == '.') {
        if ((base == 10 && isdigit(end[1])) ||
            (base == 16 && isxdigit(end[1]))) {
            goto ret_flt;
        }
    } else if (base == 10 && (*end == 'e' || *end == 'E')) {
        goto ret_flt;
    } else if (base == 16 && (*end == 'p' || *end == 'P')) {
        goto ret_flt;
    }
    set_int(fp-1, i);
    return 1;
ret_flt:
    set_flt(fp-1, riff_strtod(fp->s->str, &end, base));
    return 1;
}

// print(...)
LIB_FN(print) {
    for (int i = 0; i < argc; ++i) {
        if (i) fputs(" ", stdout);
        fputs_val(stdout, fp+i);
    }
    fputs("\n", stdout);
    set_int(fp-1, argc);
    return 1;
}

// type(x)
LIB_FN(type) {
    if (riff_unlikely(!argc)) {
        return 0;
    }
    char *str;
    size_t len = 0;
    switch (fp->type) {
    case TYPE_NULL:  str = "null";     len = 4; break;
    case TYPE_INT:   str = "int";      len = 3; break;
    case TYPE_FLOAT: str = "float";    len = 5; break;
    case TYPE_STR:   str = "string";   len = 6; break;
    case TYPE_REGEX: str = "regex";    len = 5; break;
    case TYPE_FILE:  str = "file";     len = 4; break;
    case TYPE_RANGE: str = "range";    len = 5; break;
    case TYPE_TAB:   str = "table";    len = 5; break;
    case TYPE_RFN:
    case TYPE_CFN:   str = "function"; len = 8; break;
    default: break;
    }
    set_str(fp-1, riff_str_new(str, len));
    return 1;
}

static riff_lib_fn_reg baselib[] = {
    LIB_FN_REG(assert, 0),
    LIB_FN_REG(error,  0),
    LIB_FN_REG(eval,   1),
    LIB_FN_REG(num,    1),
    LIB_FN_REG(print,  1),
    LIB_FN_REG(type,   1),
};

void riff_lib_register_base(riff_htab *g) {
    FOREACH(baselib, i) {
        riff_htab_insert_cstr(g, baselib[i].name, &(riff_val) {TYPE_CFN, .cfn = &baselib[i].fn});
    }
}
