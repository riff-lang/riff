#ifndef MEM_H
#define MEM_H

#include <stdlib.h>

#define m_growarray(a, n, cap, sz) \
    if (cap <= n) { \
        cap = cap < 8 ? 8 : cap * 2; \
        a = realloc(a, sizeof(sz) * cap); \
    }

#define m_freestr(s) \
    free(s->str); \
    free(s);

#endif
