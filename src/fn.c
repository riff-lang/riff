#include "fn.h"

void f_init(rf_fn *f, rf_str *name) {
    f->name  = name;
    f->arity = 0;
    f->code  = malloc(sizeof(rf_code));
    c_init(f->code);
}
