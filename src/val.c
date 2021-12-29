#include "types.h"

#include "table.h"

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

rf_val *v_newtab(void) {
    rf_val *v = malloc(sizeof(rf_val));
    rf_tab *t = malloc(sizeof(rf_tab));
    t_init(t);
    *v = (rf_val) {TYPE_TAB, .u.t = t};
    return v;
}
