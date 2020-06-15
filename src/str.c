#include <stdlib.h>
#include <string.h>

#include "str.h"

void s_newstr(str_t *s, const char *start, size_t l) {
    char *str;
    str = malloc(l * sizeof(char) + 1);
    memcpy(str, start, l * sizeof(char));
    str[l] = '\0';
    s->l = l;
    s->str = str;
    free(str);
}
