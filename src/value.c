#include "conf.h"
#include "table.h"
#include "types.h"

rf_val *v_newnull(void) {
    rf_val *v = malloc(sizeof(rf_val));
    *v = (rf_val) {TYPE_NULL, .s = NULL};
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
    *v = (rf_val) {TYPE_TAB, .t = t};
    return v;
}

rf_val *v_copy(rf_val *v) {
    rf_val *copy = malloc(sizeof(rf_val));
    copy->type = v->type;
    copy->i = v->i;
    return copy;
}

void v_tostring(char *buf, rf_val *v) {
    switch (v->type) {
    case TYPE_NULL: sprintf(buf, "null");               break;
    case TYPE_INT:  sprintf(buf, "%"PRId64, v->i);      break;
    case TYPE_FLT:  sprintf(buf, FLT_PRINT_FMT, v->f);  break;
    case TYPE_STR:  sprintf(buf, "%s", v->s->str);      break;
    case TYPE_RE:   sprintf(buf, "regex: %p", v->r);    break;
    case TYPE_FH:   sprintf(buf, "file: %p", v->fh->p); break;
    case TYPE_RNG:
        sprintf(buf, "range: %"PRId64"..%"PRId64":%"PRId64,
                v->q->from, v->q->to, v->q->itvl);
        break;
    case TYPE_TAB:  sprintf(buf, "table: %p", v->t);    break;
    case TYPE_RFN:
    case TYPE_CFN:  sprintf(buf, "fn: %p", v->fn);      break;
    }
}
