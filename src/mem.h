#ifndef MEM_H
#define MEM_H

#include <stdlib.h>

#define GROW_CAP(cap) ((cap) < 8 ? 8 : (cap) * 2)
#define GROW_BLOCK(b, s) \
    realloc(b, s)

#endif
