#include "types.h"

// TODO Feel like it's overkill to explicitly malloc tagged
// ints/floats
val_t *v_newint(int_t i) {
    val_t *v = malloc(sizeof(val_t));
    v->type = TYPE_INT;
    v->u.i  = i;
    return v;
}

val_t *v_newflt(flt_t f) {
    val_t *v = malloc(sizeof(val_t));
    v->type = TYPE_FLT;
    v->u.f  = f;
    return v;
}

val_t *v_newstr(str_t *s) {
    str_t *ns = s_newstr(s->str, s->l);
    val_t *v = malloc(sizeof(val_t));
    v->type = TYPE_STR;
    v->u.s  = ns;
    return v;
}
