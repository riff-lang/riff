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
    rf_str *ns = s_newstr(s->str, s->l, 0);
    rf_val *v = malloc(sizeof(rf_val));
    *v = (rf_val) {TYPE_STR, .u.s = ns};
    v->u.s->hash = s->hash;
    return v;
}

rf_val *v_newtab(uint32_t cap) {
    rf_val *v = malloc(sizeof(rf_val));
    rf_tab *t = malloc(sizeof(rf_tab));
    t_init(t);
    if (cap > 0) {
        t->cap = cap;
        t->v = malloc(cap * sizeof(rf_val));
    }
    for (int i = 0; i < cap; ++i) {
        t->v[i] = NULL;
    }
    *v = (rf_val) {TYPE_TAB, .u.t = t};
    return v;
}

rf_val *v_copy(rf_val *v) {
    rf_val *copy = malloc(sizeof(rf_val));
    copy->type = v->type;
    copy->u = v->u;
    return copy;
}
