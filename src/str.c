#include <stdlib.h>
#include <string.h>

#include "str.h"

// djb2; source: http://www.cse.yorku.ca/~oz/hash.html
static uint64_t s_hash(const char *str) {
    uint64_t h = 5381;
    int c;
    while ((c = *str++))
        h = ((h << 5) + h) + c;
    return h;
}

void s_newstr(str_t *s, const char *start, size_t l) {
    char *str;
    str = malloc(l * sizeof(char) + 1);
    memcpy(str, start, l * sizeof(char));
    str[l] = '\0';
    s->l = l;
    s->hash = s_hash(str);
    s->str = str;
    free(str);
}
