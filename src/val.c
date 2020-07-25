#include "array.h"
#include "types.h"

val_t *v_newvoid(void) {
    val_t *v = malloc(sizeof(val_t));
    v->type = TYPE_VOID;
    v->u.s  = NULL;
    return v;
}

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
    str_t *ns = s_newstr(s->str, s->l, 0);
    val_t *v = malloc(sizeof(val_t));
    v->type = TYPE_STR;
    v->u.s  = ns;
    v->u.s->hash = s->hash;
    return v;
}

val_t *v_newarr(void) {
    val_t *v = malloc(sizeof(val_t));
    arr_t *a = malloc(sizeof(arr_t));
    a_init(a);
    v->type = TYPE_ARR;
    v->u.a  = a;
    return v;
}
