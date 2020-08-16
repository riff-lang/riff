#ifndef MEM_H
#define MEM_H

#include <stdlib.h>

#define m_growarray(a, n, cap) \
    if (cap <= n) { \
        cap = cap < 8 ? 8 : cap * 2; \
        a = realloc(a, sizeof(a) * cap); \
    }

#define m_freestr(s) \
    free(s->str); \
    free(s);

#endif
