#ifndef STR_H
#define STR_H

typedef struct {
    size_t  l;
    char   *str;
} str_t;

void s_newstr(str_t *, const char *, size_t);

#endif
