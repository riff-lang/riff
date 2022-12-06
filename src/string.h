#ifndef STRING_H
#define STRING_H

#include "conf.h"
#include "value.h"

enum riff_str_hints {
    RIFF_STR_HINT_ZERO      = 1 << 0,
    RIFF_STR_HINT_COERCIBLE = 1 << 1,
};

#define riff_str_haszero(s)   ((s)->hints & RIFF_STR_HINT_ZERO)
#define riff_str_coercible(s) ((s)->hints & RIFF_STR_HINT_COERCIBLE)

#define riff_str_eq(x,y)      ((x) == (y))
#define riff_str_eq_raw(x,y) \
    (((x)->hash == (y)->hash) && ((x)->len == (y)->len) && !memcmp((x)->str, (y)->str, (x)->len))
#define riff_str_hash(s)      ((s)->hash)
#define riff_strlen(s)        ((s)->len)

void      riff_stab_init(void);
riff_str *riff_str_new_extra(const char *, size_t, uint8_t);
riff_str *riff_str_new(const char *, size_t);
riff_str *riff_strcat(char *, char *, size_t, size_t);
riff_str *riff_substr(char *, size_t, riff_int, riff_int, riff_int);

static inline size_t riff_tostr(riff_val *v, char **buf) {
    switch (v->type) {
    case TYPE_NULL:
        *buf = riff_str_new("", 0)->str;
        return 0;
    case TYPE_INT:   return sprintf(*buf, "%"PRId64, v->i);
    case TYPE_FLOAT: return sprintf(*buf, FLT_PRINT_FMT, v->f);
    case TYPE_STR:
        *buf = v->s->str;
        return riff_strlen(v->s);
    case TYPE_REGEX: return sprintf(*buf, "regex: %p", v->r);
    case TYPE_FILE:  return sprintf(*buf, "file: %p", v->fh->p);
    case TYPE_RANGE:
        return sprintf(*buf, "range: %"PRId64"..%"PRId64":%"PRId64,
                v->q->from, v->q->to, v->q->itvl);
    case TYPE_TAB:   return sprintf(*buf, "table: %p", v->t);
    case TYPE_RFN:
    case TYPE_CFN:   return sprintf(*buf, "fn: %p", v->fn);
    }
    return 0;
}

#endif
