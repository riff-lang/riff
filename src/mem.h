#ifndef MEM_H
#define MEM_H

#include <stdlib.h>

// Doubles the size of a given array's allocation if already at
// capacity
#define m_growarray(a, n, cap, sz) \
    if (cap <= n) { \
        cap = cap < 8 ? 8 : cap * 2; \
        a = realloc(a, sizeof(sz) * cap); \
    }

// Resizes a given buffer to the specified size ONLY IF the buffer
// does not have enough space
#define m_resizebuffer(b, n, cap, sz) \
    if (cap < n) { \
        cap = n; \
        b = realloc(b, sizeof(sz) * cap); \
    }

#define m_freestr(s) free(s->str); free(s);

#endif
