#include "table.h"
#include "types.h"

rf_val *v_newnull(void) {
    rf_val *v = malloc(sizeof(rf_val));
    *v = (rf_val) {TYPE_NULL, .u.s = NULL};
    return v;
}

rf_val *v_newtab(uint32_t cap) {
    rf_val *v = malloc(sizeof(rf_val));
    rf_tab *t = malloc(sizeof(rf_tab));
    t_init(t);
    if (cap > 0) {
        t->cap = cap;
        t->v = calloc(cap, sizeof(rf_val *));
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
