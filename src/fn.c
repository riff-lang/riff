#include "fn.h"

void riff_fn_init(riff_fn *f) {
    f->name  = NULL;
    f->arity = 0;
    c_init(&f->code);
}
