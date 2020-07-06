#ifndef MEM_H
#define MEM_H

#include <stdlib.h>

#define eval_resize(a, n, cap) \
    if (cap <= n) { \
        cap = cap < 8 ? 8 : cap * 2; \
        a = realloc(a, sizeof(a) * cap); \
    }

// Evaluate if array needs to be reallocated, considering a given load
// factor
#define eval_resize_lf(a, n, cap, lf) \
    if ((cap * lf) <= n) { \
        cap = cap < 8 ? 8 : cap * 2; \
        a = realloc(a, sizeof(a) * cap); \
    }

#endif
