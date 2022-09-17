#include "conf.h"
#include "table.h"
#include "value.h"

riff_val *v_newnull(void) {
    riff_val *v = malloc(sizeof(riff_val));
    *v = (riff_val) {TYPE_NULL, .s = NULL};
    return v;
}

riff_val *v_newtab(uint32_t cap) {
    riff_val *v = malloc(sizeof(riff_val));
    riff_tab *t = malloc(sizeof(riff_tab));
    riff_tab_init(t);
    if (cap > 0) {
        t->cap = cap;
        t->v = calloc(cap, sizeof(riff_val *));
    }
    *v = (riff_val) {TYPE_TAB, .t = t};
    return v;
}

riff_val *v_copy(riff_val *v) {
    riff_val *copy = malloc(sizeof(riff_val));
    copy->type = v->type;
    copy->i = v->i;
    return copy;
}
