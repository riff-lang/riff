#include <stdlib.h>
#include <string.h>

#include "types.h"

// djb2; source: http://www.cse.yorku.ca/~oz/hash.html
static uint32_t s_hash(const char *str) {
    uint32_t h = 5381;
    unsigned int c;
    while ((c = *str++))
        h = ((h << 5) + h) + c;
    return h;
}

str_t *s_newstr(const char *start, size_t l) {
    char *str;
    str = malloc(l * sizeof(char) + 1);
    memcpy(str, start, l * sizeof(char));
    str[l] = '\0';
    str_t *s = NULL;
    s = malloc(sizeof(str_t) + 1);
    s->l = l;
    s->hash = s_hash(str);
    s->str = str;
    return s;
}
