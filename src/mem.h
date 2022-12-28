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

#endif
