#ifndef MEM_H
#define MEM_H

#include <stdlib.h>

#define increase_cap(cap) ((cap) < 8 ? 8 : (cap) * 2)
#define grow_array(a, s)  (realloc(b, s))

#endif
