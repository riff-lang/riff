#include "table.h"
#include "types.h"

rf_val *v_newnull(void) {
    rf_val *v = malloc(sizeof(rf_val));
    *v = (rf_val) {TYPE_NULL, .u.s = NULL};
    return v;
}

rf_val *v_newint(rf_int i) {
    rf_val *v = malloc(sizeof(rf_val));
    *v = (rf_val) {TYPE_INT, .u.i = i};
    return v;
}

rf_val *v_newflt(rf_flt f) {
    rf_val *v = malloc(sizeof(rf_val));
    *v = (rf_val) {TYPE_FLT, .u.f = f};
    return v;
}

rf_val *v_newstr(rf_str *s) {
    rf_str *ns   = s_newstr(s->str, s->l, 0);
    rf_val *v    = malloc(sizeof(rf_val));
    *v = (rf_val) {TYPE_STR, .u.s = ns};
    v->u.s->hash = s->hash;
    return v;
}

rf_val *v_newtbl(void) {
    rf_val *v = malloc(sizeof(rf_val));
    rf_tbl *t = malloc(sizeof(rf_tbl));
    t_init(t);
    *v = (rf_val) {TYPE_TBL, .u.t = t};
    return v;
}
