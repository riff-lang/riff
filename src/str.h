#ifndef STR_H
#define STR_H

#include <stdint.h>

typedef struct {
    size_t    l;
    uint32_t  hash;
    char     *str;
} str_t;

str_t *s_newstr(const char *, size_t);

#endif
