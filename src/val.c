#include "array.h"
#include "types.h"

rf_val *v_newnull(void) {
    rf_val *v = malloc(sizeof(rf_val));
    v->type   = TYPE_NULL;
    v->u.s    = NULL;
    return v;
}

rf_val *v_newint(rf_int i) {
    rf_val *v = malloc(sizeof(rf_val));
    v->type   = TYPE_INT;
    v->u.i    = i;
    return v;
}

rf_val *v_newflt(rf_flt f) {
    rf_val *v = malloc(sizeof(rf_val));
    v->type   = TYPE_FLT;
    v->u.f    = f;
    return v;
}

rf_val *v_newstr(rf_str *s) {
    rf_str *ns   = s_newstr(s->str, s->l, 0);
    rf_val *v    = malloc(sizeof(rf_val));
    v->type      = TYPE_STR;
    v->u.s       = ns;
    v->u.s->hash = s->hash;
    return v;
}

rf_val *v_newarr(void) {
    rf_val *v = malloc(sizeof(rf_val));
    rf_arr *a = malloc(sizeof(rf_arr));
    a_init(a);
    v->type = TYPE_ARR;
    v->u.a  = a;
    return v;
}
