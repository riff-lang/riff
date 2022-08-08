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

void riff_tostr(riff_val *v, char *buf) {
    switch (v->type) {
    case TYPE_NULL: sprintf(buf, "null");               break;
    case TYPE_INT:  sprintf(buf, "%"PRId64, v->i);      break;
    case TYPE_FLOAT:  sprintf(buf, FLT_PRINT_FMT, v->f);  break;
    case TYPE_STR:  sprintf(buf, "%s", v->s->str);      break;
    case TYPE_REGEX:   sprintf(buf, "regex: %p", v->r);    break;
    case TYPE_FILE:   sprintf(buf, "file: %p", v->fh->p); break;
    case TYPE_RANGE:
        sprintf(buf, "range: %"PRId64"..%"PRId64":%"PRId64,
                v->q->from, v->q->to, v->q->itvl);
        break;
    case TYPE_TAB:  sprintf(buf, "table: %p", v->t);    break;
    case TYPE_RFN:
    case TYPE_CFN:  sprintf(buf, "fn: %p", v->fn);      break;
    }
}
