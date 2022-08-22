#ifndef STRING_H
#define STRING_H

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
riff_str *riff_str_new_concat(char *, char *);
riff_str *riff_substr(char *, riff_int, riff_int, riff_int);

#endif
