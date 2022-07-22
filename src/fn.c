#include "fn.h"

void f_init(riff_fn *f, riff_str *name) {
    f->name  = name;
    f->arity = 0;
    f->code  = malloc(sizeof(riff_code));
    c_init(f->code);
}
