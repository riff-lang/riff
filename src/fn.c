#include "fn.h"

void riff_fn_init(riff_fn *f, riff_str *name) {
    f->name  = name;
    f->arity = 0;
    c_init(&f->code);
}
