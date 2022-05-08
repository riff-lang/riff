#ifndef PRNG_H
#define PRNG_H

#include "types.h"

typedef struct {
    rf_uint u[4];
} prng_state;

rf_uint prng_next(prng_state *);
void prng_seed(prng_state *, rf_uint);

#endif
